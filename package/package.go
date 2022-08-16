package xgenerics

import (
	"github.com/jurgen-kluft/ccode/denv"
	"github.com/jurgen-kluft/xbase/package"
	"github.com/jurgen-kluft/xentry/package"
)

// GetPackage returns the package object of 'xgenerics'
func GetPackage() *denv.Package {
	// Dependencies
	unittestpkg := xunittest.GetPackage()
	entrypkg := xentry.GetPackage()
	xbasepkg := xbase.GetPackage()

	// The main (xgenerics) package
	mainpkg := denv.NewPackage("xgenerics")
	mainpkg.AddPackage(unittestpkg)
	mainpkg.AddPackage(entrypkg)
	mainpkg.AddPackage(xbasepkg)

	// 'xgenerics' library
	mainlib := denv.SetupDefaultCppLibProject("xgenerics", "github.com\\jurgen-kluft\\xgenerics")
	mainlib.Dependencies = append(mainlib.Dependencies, xbasepkg.GetMainLib())

	// 'xgenerics' unittest project
	maintest := denv.SetupDefaultCppTestProject("xgenerics_test", "github.com\\jurgen-kluft\\xgenerics")
	maintest.Dependencies = append(maintest.Dependencies, unittestpkg.GetMainLib())
	maintest.Dependencies = append(maintest.Dependencies, entrypkg.GetMainLib())
	maintest.Dependencies = append(maintest.Dependencies, xbasepkg.GetMainLib())
	maintest.Dependencies = append(maintest.Dependencies, mainlib)

	mainpkg.AddMainLib(mainlib)
	mainpkg.AddUnittest(maintest)
	return mainpkg
}
