// 2018, Georg Sauthoff <mail@gms.tf>
// SPDX-License-Identifier: GPL-3.0-or-later

// How to build:
//
// sudo dnf install golang-github-golang-sys-devel
// export GOPATH=$HOME/go:/usr/share/gocode
// mkdir build
// cd build
// go build -o searchb-go ../searchb.go


package main

import (
  "golang.org/x/sys/unix"
  "bytes"
  "fmt"
  "os"
)

func mmap(name string) ([]byte, error) {
  f, err := os.Open(name)
  if err != nil {
    return nil, err
  }
  st, err := f.Stat()
  if err != nil {
    f.Close()
    return nil, err
  }
  if st.Size() == 0 {
    f.Close()
    return nil, nil // nil is also the empty byte slice
  }
  m, err := unix.Mmap(int(f.Fd()), 0, int(st.Size()), unix.PROT_READ,
                      unix.MAP_SHARED)
  if err != nil {
    f.Close()
    return nil, err
  }
  err = f.Close()
  if err != nil {
    unix.Munmap(m)
    return nil, err
  }
  return m, err
}

func main() {
  q, err := mmap(os.Args[1])
  if err != nil {
    panic(err.Error())
  }
  t, err := mmap(os.Args[2])
  if err != nil {
    panic(err.Error())
  }
  i := bytes.Index(t, q)
  if i == -1 {
    os.Exit(1)
  }
  fmt.Println(i)
  os.Exit(0)
}
