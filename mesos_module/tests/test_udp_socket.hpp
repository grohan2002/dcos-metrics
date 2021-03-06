#pragma once

#include <boost/asio.hpp>
#include <glog/logging.h>

class AbstractTestUDPSocket {
 public:
  AbstractTestUDPSocket()
    : svc(), socket(svc), listener_endpoint() { }

  virtual ~AbstractTestUDPSocket() {
    socket.close();
  }

 protected:
  const boost::asio::ip::address LOCALHOST =
    boost::asio::ip::address::from_string("127.0.0.1");

  boost::asio::io_service svc;
  boost::asio::ip::udp::socket socket;
  boost::asio::ip::udp::endpoint listener_endpoint;
};


class TestUDPWriteSocket : public AbstractTestUDPSocket {
 public:
  void connect(size_t port) {
    listener_endpoint = boost::asio::ip::udp::endpoint(LOCALHOST, port);
    socket.open(listener_endpoint.protocol());
    LOG(INFO) << "(TEST) Configured for communication to destination "
              << "endpoint[" << listener_endpoint << "]";
  }

  void write(const std::string& data) {
    socket.send_to(boost::asio::buffer(data), listener_endpoint);
    LOG(INFO) << "(TEST) Sent message[" << data << "] from "
              << "port[" << socket.local_endpoint().port() << "] to "
              << "destination[" << listener_endpoint << "]";
  }
};


class TestUDPReadSocket : public AbstractTestUDPSocket {
 public:
  TestUDPReadSocket()
    : AbstractTestUDPSocket(),
      timeout_deadline(svc),
      buffer_size(65536) {
    buffer = (char*) malloc(buffer_size);
    check_deadline(); // start timer
  }
  virtual ~TestUDPReadSocket() {
    free(buffer);
  }

  size_t listen(size_t port = 0) {
    return listen(LOCALHOST, port);
  }

  size_t listen(boost::asio::ip::address host, size_t port) {
    if (listener_endpoint.port() != 0 && listener_endpoint.port() == port) {
      return listener_endpoint.port();
    }

    boost::asio::ip::udp::endpoint requested_endpoint(host, port);
    socket.open(requested_endpoint.protocol());
    socket.bind(requested_endpoint);
    listener_endpoint = socket.local_endpoint();
    LOG(INFO) << "(TEST) Listening on endpoint[" << listener_endpoint << "]";
    return listener_endpoint.port();
  }

  size_t available() {
    return socket.available();
  }

  std::string read(size_t timeout_ms = 100) {
    timeout_deadline.expires_from_now(boost::posix_time::milliseconds(timeout_ms));

    boost::system::error_code ec = boost::asio::error::would_block;
    size_t len = 0;

    boost::asio::ip::udp::endpoint sender_endpoint;
    socket.async_receive_from(boost::asio::buffer(buffer, buffer_size),
        sender_endpoint,
        std::bind(&TestUDPReadSocket::recv_cb, std::placeholders::_1, std::placeholders::_2, &ec, &len));

    while (ec == boost::asio::error::would_block) {
      svc.poll();
    }

    if (ec == boost::system::errc::operation_canceled) {
      LOG(INFO) << "(TEST) Timed out waiting " << timeout_ms << "ms for message on "
                << "endpoint[" << listener_endpoint << "]";
      return "";
    } else {
      std::ostringstream oss;
      oss.write(buffer, len);
      LOG(INFO) << "(TEST) Got message[" << oss.str() << "] on "
                << "endpoint[" << listener_endpoint << "] from "
                << "sender[" << sender_endpoint << "]";
      return oss.str();
    }
  }

 private:
  void check_deadline() {
    if (timeout_deadline.expires_at() <= boost::asio::deadline_timer::traits_type::now()) {
      // timeout occurred, end any receive() calls
      socket.cancel();
      timeout_deadline.expires_at(boost::posix_time::pos_infin);
    }
    timeout_deadline.async_wait(std::bind(&TestUDPReadSocket::check_deadline, this));
  }

  static void recv_cb(
      const boost::system::error_code& ec, size_t len,
      boost::system::error_code* out_ec, size_t* out_len) {
    // pass read outcome/size upstream
    *out_ec = ec;
    *out_len = len;
  }

  boost::asio::deadline_timer timeout_deadline;
  const size_t buffer_size;
  char* buffer;
};
