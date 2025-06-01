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
	mainpkg := denv.NewPackage("github.com\\jurgen-kluft", "cgenerics")
	mainpkg.AddPackage(unittestpkg)
	mainpkg.AddPackage(cbasepkg)

	// 'cgenerics' library
	mainlib := denv.SetupCppLibProject(mainpkg, "cgenerics")
	mainlib.AddDependencies(cbasepkg.GetMainLib()...)

	// 'cgenerics' unittest project
	maintest := denv.SetupCppTestProject(mainpkg, "cgenerics_test")
	maintest.AddDependencies(unittestpkg.GetMainLib()...)
	maintest.AddDependencies(cbasepkg.GetMainLib()...)
	maintest.AddDependency(mainlib)

	mainpkg.AddMainLib(mainlib)
	mainpkg.AddUnittest(maintest)
	return mainpkg
}
