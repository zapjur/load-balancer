package main

import (
	"bufio"
	"fmt"
	"math/rand"
	"net"
	"sync"
	"time"
)

const (
	simplePort = 8080
	echoPort   = 8081

	simpleRate = 1 * time.Second
	echoRate   = 3 * time.Second
)

var echoCounter int
var mu sync.Mutex

func sendSimpleLoop() {
	counter := 0
	for {
		counter++
		conn, err := net.Dial("tcp", fmt.Sprintf("localhost:%d", simplePort))
		if err != nil {
			fmt.Printf("[SIMPLE] Conn error: %v\n", err)
			time.Sleep(simpleRate)
			continue
		}

		message := fmt.Sprintf("Simple msg %d\n", counter)
		fmt.Fprintf(conn, message)

		resp, err := bufio.NewReader(conn).ReadString('\n')
		if err == nil {
			fmt.Printf("[SIMPLE] Resp: %s", resp)
		} else {
			fmt.Printf("[SIMPLE] Read error: %v\n", err)
		}

		conn.Close()
		time.Sleep(simpleRate)
	}
}

func echoPersistentClient(id int) {
	conn, err := net.Dial("tcp", fmt.Sprintf("localhost:%d", echoPort))
	if err != nil {
		fmt.Printf("[ECHO %d] Conn error: %v\n", id, err)
		return
	}
	defer conn.Close()

	reader := bufio.NewReader(conn)
	ticker := time.NewTicker(echoRate)
	defer ticker.Stop()

	counter := 0

	for {
		select {
		case <-ticker.C:
			counter++
			msg := fmt.Sprintf("Echo %d from client %d\n", counter, id)
			_, err := fmt.Fprintf(conn, msg)
			if err != nil {
				fmt.Printf("[ECHO %d] Write error: %v\n", id, err)
				return
			}

			resp, err := reader.ReadString('\n')
			if err != nil {
				fmt.Printf("[ECHO %d] Read error: %v\n", id, err)
				return
			}
			fmt.Printf("[ECHO %d] Resp: %s", id, resp)

			if rand.Float64() < 0.1 {
				fmt.Printf("[ECHO %d] Random disconnect\n", id)
				return
			}
		}
	}
}

func main() {
	rand.Seed(time.Now().UnixNano())

	go sendSimpleLoop()

	for {
		mu.Lock()
		echoCounter++
		id := echoCounter
		mu.Unlock()

		go echoPersistentClient(id)
		time.Sleep(1 * time.Second)
	}
}
