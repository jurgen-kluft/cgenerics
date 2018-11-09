package main

import (
	"github.com/jurgen-kluft/xcode"
	"github.com/jurgen-kluft/xgenerics/package"
)

func main() {
	xcode.Generate(xgenerics.GetPackage())
}
