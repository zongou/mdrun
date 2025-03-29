package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/gomarkdown/markdown/ast"
	"github.com/gomarkdown/markdown/parser"
)

type Commands struct {
	Heading     ast.Heading
	CodeBlocks  []ast.CodeBlock
	SubCommands []Commands
	// env
}

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

func parseCommands(doc ast.Node) []Commands {
	var commands []Commands
	var stack []*Commands // Track current heading hierarchy

	ast.WalkFunc(doc, func(node ast.Node, entering bool) ast.WalkStatus {
		if !entering {
			return ast.GoToNext
		}

		switch v := node.(type) {
		case *ast.Heading:
			cmd := Commands{}
			cmd.Heading = *v

			// Pop stack until we find appropriate parent level
			for len(stack) > 0 && stack[len(stack)-1].Heading.Level >= v.Level {
				stack = stack[:len(stack)-1]
			}

			if len(stack) == 0 {
				commands = append(commands, cmd)
				stack = append(stack, &commands[len(commands)-1])
			} else {
				parent := stack[len(stack)-1]
				parent.SubCommands = append(parent.SubCommands, cmd)
				stack = append(stack, &parent.SubCommands[len(parent.SubCommands)-1])
			}

		case *ast.CodeBlock:
			if len(stack) > 0 {
				current := stack[len(stack)-1]
				current.CodeBlocks = append(current.CodeBlocks, *v)
			}
		}
		return ast.GoToNext
	})

	return commands
}

func printCommands(cmds []Commands, level int) {
	indent := strings.Repeat("  ", level)
	for _, cmd := range cmds {
		heading := ""
		if len(cmd.Heading.Children) > 0 {
			if txt, ok := cmd.Heading.Children[0].(*ast.Text); ok {
				heading = string(txt.Literal)
			}
		}
		fmt.Printf("%s%s (Level %d)\n", indent, heading, cmd.Heading.Level)

		for _, block := range cmd.CodeBlocks {
			fmt.Printf("%s  Code[%s]: %q\n", indent, block.Info, string(block.Literal))
		}

		printCommands(cmd.SubCommands, level+1)
	}
}

// findReadme searches for a README.md file in the current or parent directories.
func findReadme() (string, error) {
	dir, err := os.Getwd()
	if err != nil {
		return "", err
	}

	for {
		files, err := os.ReadDir(dir)
		if err != nil {
			return "", err
		}

		for _, file := range files {
			// Check for "README.md" ignoring case
			if !file.IsDir() && strings.EqualFold(file.Name(), "README.md") {
				return filepath.Join(dir, file.Name()), nil
			}
		}

		parent := filepath.Dir(dir)
		if parent == dir { // Reached the root directory
			break
		}
		dir = parent
	}

	return "", fmt.Errorf("README.md not found")
}

func getHeadingText(heading ast.Heading) string {
	if len(heading.Children) > 0 {
		if txt, ok := heading.Children[0].(*ast.Text); ok {
			return string(txt.Literal)
		}
	}
	return ""
}

func executeCodeBlock(block ast.CodeBlock, args []string) error {
	fmt.Printf("Executing code block [%s] with args %v:\n%s\n",
		block.Info, args, string(block.Literal))
	// TODO: implement actual code block execution with args
	return nil
}

func findAndExecuteNestedCommand(cmds []Commands, path []string, args []string, currentDepth int) bool {
	if currentDepth >= len(path) {
		return false
	}

	targetHeading := path[currentDepth]
	for _, cmd := range cmds {
		heading := getHeadingText(cmd.Heading)

		if heading == targetHeading {
			if currentDepth == len(path)-1 {
				// Execute all code blocks under this heading with args
				for _, block := range cmd.CodeBlocks {
					if err := executeCodeBlock(block, args); err != nil {
						fmt.Printf("Error executing block: %v\n", err)
						return false
					}
				}
				return true
			}
			// Continue searching in subcommands
			if findAndExecuteNestedCommand(cmd.SubCommands, path, args, currentDepth+1) {
				return true
			}
		}
	}
	return false
}

func main() {
	fileFlag := flag.String("f", "", "Path to the markdown file")
	flag.Parse()

	args := flag.Args()
	if len(args) < 1 {
		fmt.Println("Usage: markdown-build [-f <input-file>] <heading-path...> [-- <args...>]")
		return
	}

	// Split args into heading path and code block args
	var headingPath []string
	var codeArgs []string
	for i, arg := range args {
		if arg == "--" {
			headingPath = args[:i]
			codeArgs = args[i+1:]
			break
		}
	}
	if len(codeArgs) == 0 { // No "--" found
		headingPath = args
	}

	var inputFile string
	if *fileFlag != "" {
		inputFile = *fileFlag
	} else {
		var err error
		inputFile, err = findReadme()
		if err != nil {
			fmt.Println("Error:", err)
			fmt.Println("Usage: markdown-build [-f <input-file>]")
			return
		}
	}

	content, err := os.ReadFile(inputFile)
	if err != nil {
		fmt.Printf("Error reading file: %v\n", err)
		return
	}

	extensions := parser.CommonExtensions | parser.AutoHeadingIDs | parser.NoEmptyLineBeforeBlock
	p := parser.NewWithExtensions(extensions)
	doc := p.Parse(content)

	commands := parseCommands(doc)
	if !findAndExecuteNestedCommand(commands, headingPath, codeArgs, 0) {
		fmt.Printf("Command path '%s' not found\n", strings.Join(headingPath, " > "))
		return
	}
}
