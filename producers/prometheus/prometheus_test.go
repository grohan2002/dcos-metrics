//+build unit

// Copyright 2016 Mesosphere, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package prometheus

import (
	"io/ioutil"
	"net"
	"net/http"
	"os"
	"strconv"
	"testing"
	"time"

	"github.com/dcos/dcos-metrics/producers"
	. "github.com/smartystreets/goconvey/convey"
)

func TestNew(t *testing.T) {
	Convey("The New method should return a producer and a channel", t, func() {
		pi, pc := New(Config{})
		So(pi, ShouldHaveSameTypeAs, &promProducer{})
		So(pc, ShouldHaveSameTypeAs, make(chan producers.MetricsMessage))
	})
}

func TestRun(t *testing.T) {
	Convey("Should create a Prometheus server on the configured port", t, func() {
		// Ensure the environment is clean
		if err := os.Unsetenv("LISTEN_PID"); err != nil {
			panic(err)
		}
		if err := os.Unsetenv("LISTEN_FDS"); err != nil {
			panic(err)
		}

		port, err := getEphemeralPort()
		if err != nil {
			panic(err)
		}

		p, metricsChan := New(Config{Port: port})
		go p.Run()
		time.Sleep(1 * time.Second)

		// The web server should be listening on the configured TCP port
		_, err = net.Dial("tcp", net.JoinHostPort("localhost", strconv.Itoa(port)))
		So(err, ShouldBeNil)

		// feed in some fake data
		dps := []producers.Datapoint{
			producers.Datapoint{
				Name:  "datapoint-one",
				Value: 123.456,
			},
		}
		metricsChan <- producers.MetricsMessage{
			Name:       "some-message",
			Datapoints: dps,
			Timestamp:  time.Now().UTC().Unix(),
		}
		time.Sleep(250 * time.Millisecond)

		resp, err := http.Get("http://" + net.JoinHostPort("localhost", strconv.Itoa(port)) + "/metrics")
		So(err, ShouldBeNil)
		defer resp.Body.Close()

		body, err := ioutil.ReadAll(resp.Body)
		So(err, ShouldBeNil)

		// TODO (philipnrmn): verify the output better than this
		So(string(body), ShouldContainSubstring, "datapoint_one")
		So(string(body), ShouldContainSubstring, "123.456")

	})
}

// getEphemeralPort returns an available ephemeral port on the system.
func getEphemeralPort() (int, error) {
	addr, err := net.ResolveTCPAddr("tcp", "localhost:0")
	if err != nil {
		return 0, err
	}

	l, err := net.ListenTCP("tcp", addr)
	if err != nil {
		return 0, err
	}
	defer l.Close()

	return l.Addr().(*net.TCPAddr).Port, nil
}
