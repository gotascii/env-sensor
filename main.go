package main

import (
	"bufio"
	"bytes"
	"fmt"
	"io/ioutil"
	"log"
	"net"
	"net/http"
	"os"
	"strings"
	"time"
)

const authHeader string = "Authorization"
const contentTypeHeader string = "Content-Type"
const contentType string = "application/json"
const method string = "POST"

var url string = os.Getenv("URL")
var auth string = fmt.Sprintf("Bearer %s", os.Getenv("AUTH"))

var names []string = []string{
	"ps01", "ps25", "ps10", "pe01", "pe25", "pe10",
	"pu03", "pu05", "pu01", "pu25", "pu50", "pu10",
	"tvoc", "ecot", "temp", "humi", "pres", "airm",
}

func post(name, val string) {
	ts := time.Now().Unix()
	body := `[{"name": "env.%s", "interval": 1, "time": %d, "value": %s}]`
	body = fmt.Sprintf(body, name, ts, val)

	client := &http.Client{}
	b := bytes.NewBuffer([]byte(body))
	req, err := http.NewRequest(method, url, b)
	if err != nil {
		log.Println(err)
	}

	req.Header.Set(authHeader, auth)
	req.Header.Set(contentTypeHeader, contentType)
	resp, err := client.Do(req)
	if err != nil {
		log.Println(err)
		return
	}

	f, err := ioutil.ReadAll(resp.Body)
	resp.Body.Close()
	if err != nil {
		log.Println(err)
		log.Println(string(f))
		return
	}
}

func send(str string) {
	split := strings.Split(str, ",")
	if len(split) != len(names) {
		log.Printf("received malformed metrics: %s\n", str)
		return
	}
	for i, name := range names {
		go post(name, split[i])
	}
	log.Println(str)
}

func handle(conn net.Conn) {
	// Each conn gets its own scanner
	scanner := bufio.NewScanner(conn)
	// Create a channel for scanner Text()
	text := make(chan string)
	// Create a 10 second timeout channel
	timeout := time.NewTimer(10 * time.Second)

	// Start scanning in a go routine
	go func() {
		for scanner.Scan() {
			// Reset the timeout timer if Scan() was successful
			if !timeout.Stop() {
				<-timeout.C
			}
			timeout.Reset(10 * time.Second)
			// Send the Text()
			text <- scanner.Text()
		}
	}()

	for {
		select {
		case metrics := <-text:
			go send(metrics)
		case <-timeout.C:
			conn.Close()
			log.Println("connection timed out")
			return
		}
	}
}

func main() {
	listener, err := net.Listen("tcp", ":8080")
	if err != nil {
		panic(err)
	}
	defer listener.Close()
	log.Printf("listening for connections on %s", listener.Addr().String())

	for {
		conn, err := listener.Accept()
		if err != nil {
			log.Printf("error accepting connection from client: %s", err)
		} else {
			log.Println("connection accepted")
			go handle(conn)
		}
	}
}
