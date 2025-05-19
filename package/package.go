package cgenerics

import (
	cbase "github.com/jurgen-kluft/cbase/package"
	"github.com/jurgen-kluft/ccode/denv"
	cunittest "github.com/jurgen-kluft/cunittest/package"
)

// GetPackage returns the package object of 'cgenerics'
func GetPackage() *denv.Package {
	// Dependencies
	unittestpkg := cunittest.GetPackage()
	cbasepkg := cbase.GetPackage()

	// The main (cgenerics) package
	mainpkg := denv.NewPackage("cgenerics")
	mainpkg.AddPackage(unittestpkg)
	mainpkg.AddPackage(cbasepkg)

	// 'cgenerics' library
	mainlib := denv.SetupCppLibProject("cgenerics", "github.com\\jurgen-kluft\\cgenerics")
	mainlib.AddDependencies(cbasepkg.GetMainLib()...)

	// 'cgenerics' unittest project
	maintest := denv.SetupDefaultCppTestProject("cgenerics_test", "github.com\\jurgen-kluft\\cgenerics")
	maintest.AddDependencies(unittestpkg.GetMainLib()...)
	maintest.AddDependencies(cbasepkg.GetMainLib()...)
	maintest.Dependencies = append(maintest.Dependencies, mainlib)

	mainpkg.AddMainLib(mainlib)
	mainpkg.AddUnittest(maintest)
	return mainpkg
}
