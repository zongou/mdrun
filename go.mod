module mdrun

go 1.23.5

require (
	github.com/GoToUse/treeprint v0.0.0-20230314143140-b9b91db455f6
	github.com/gomarkdown/markdown v0.0.0-20250311123330-531bef5e742b
	github.com/posener/complete/v2 v2.1.0
)

require (
	github.com/mattn/go-colorable v0.1.13 // indirect
	github.com/mattn/go-isatty v0.0.20 // indirect
	github.com/posener/script v1.2.0 // indirect
)

require (
	github.com/fatih/color v1.18.0
	golang.org/x/sys v0.31.0 // indirect
)

replace github.com/posener/complete/v2 => ./complete
