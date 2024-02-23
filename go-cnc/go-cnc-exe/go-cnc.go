package main

import _ "example.com/go-cnc/libgcode"
import _ "example.com/go-cnc/machine/control"
import "example.com/go-cnc/machine/application"

// $ go get github.com/gin-gonic/gin
// $ go build go-cnc-exe/go-cnc.go 
//import _ "github.com/gin-gonic/gin"

import "fmt"
import "time"

func main() {
	machine_application.AlbumServer()
	machine_application.ReactServer()
	fmt.Println("Hello, world!")
	for {
		time.Sleep(90 * time.Second)
	}
	fmt.Println("Farewell, world!")
}
