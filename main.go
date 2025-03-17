package main

import (
	"fmt"
	"os"
	"strings"

	"github.com/gomarkdown/markdown/ast"
	"github.com/gomarkdown/markdown/parser"
)

func printNode(node ast.Node, level int) {
	indent := strings.Repeat("  ", level)
	fmt.Printf("%s%T:", indent, node)

	switch v := node.(type) {
	case *ast.Document:
		fmt.Printf(" (document)")
	case *ast.Heading:
		fmt.Printf(" (level %d)", v.Level)
		if len(v.Children) > 0 {
			if txt, ok := v.Children[0].(*ast.Text); ok {
				fmt.Printf(" %q", txt.Literal)
			}
		}
	case *ast.Text:
		fmt.Printf(" %q", v.Literal)
	case *ast.CodeBlock:
		fmt.Printf(" [%s]", v.Info)
		fmt.Printf(" %q", string(v.Literal))
	case *ast.Link:
		fmt.Printf(" [dest=%s]", v.Destination)
		if v.Title != nil {
			fmt.Printf(" title=%q", v.Title)
		}
	case *ast.List:
		// fmt.Printf(" (ordered=%v)", v.IsOrdered)
	case *ast.Table:
		fmt.Printf(" (table)")
	case *ast.HTMLBlock:
		fmt.Printf(" %q", v.Literal)
	case *ast.Paragraph:
		fmt.Printf(" (paragraph)")
	case *ast.Image:
		fmt.Printf(" [src=%s]", v.Destination)
	case *ast.Strong:
		fmt.Printf(" (strong)")
	case *ast.Emph:
		fmt.Printf(" (emphasis)")
	}
	fmt.Println()

	for _, child := range node.GetChildren() {
		printNode(child, level+1)
	}
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: markdown-build <input-file>")
		return
	}

	content, err := os.ReadFile(os.Args[1])
	if err != nil {
		fmt.Printf("Error reading file: %v\n", err)
		return
	}

	extensions := parser.CommonExtensions | parser.AutoHeadingIDs | parser.NoEmptyLineBeforeBlock
	p := parser.NewWithExtensions(extensions)
	doc := p.Parse(content)

	fmt.Println("AST Structure:")
	fmt.Println("-------------")
	printNode(doc, 0)
}
