package main

import (
	"os";
	"runtime";
	"sync";
	"time";
)

func main() {
	const numGoroutines = 10;
	const gomaxprocs = 200;
	const sleepms = 200;
	runtime.GOMAXPROCS(gomaxprocs);
	var m sync.Mutex;
	c := make(chan int);
	work := func(i int) {
		m.Lock();
		c <- i;
		m.Unlock();
	};
	m.Lock();
	for i := 0; i < numGoroutines; i++ {
		go work(i);
		time.Sleep(sleepms * 1000 * 1000);
	}
	m.Unlock();	// Let the goroutines wake up.
	ok := true;
	for i := 0; i < numGoroutines; i++ {
		got := <-c;
		if got != i {
			ok = false;
		}
		print(got, ",");
	}
	print(" ok=", ok, "\n");
	if !ok {
		os.Exit(1);
	}
}
