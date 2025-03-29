package main

import (
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/gomarkdown/markdown/ast"
	"github.com/gomarkdown/markdown/parser"
)

type cmdNode struct {
	Heading    ast.Heading
	CodeBlocks []ast.CodeBlock
	Children   []cmdNode
	// Description []string
	// env
}

// func printNode(node ast.Node, level int) {
// 	indent := strings.Repeat("  ", level)
// 	fmt.Printf("%s%T:", indent, node)

// 	switch v := node.(type) {
// 	case *ast.Document:
// 		fmt.Printf(" (document)")
// 	case *ast.Heading:
// 		fmt.Printf(" (level %d)", v.Level)
// 		if len(v.Children) > 0 {
// 			if txt, ok := v.Children[0].(*ast.Text); ok {
// 				fmt.Printf(" %q", txt.Literal)
// 			}
// 		}
// 	case *ast.Text:
// 		fmt.Printf(" %q", v.Literal)
// 	case *ast.CodeBlock:
// 		fmt.Printf(" [%s]", v.Info)
// 		fmt.Printf(" %q", string(v.Literal))
// 	case *ast.Link:
// 		fmt.Printf(" [dest=%s]", v.Destination)
// 		if v.Title != nil {
// 			fmt.Printf(" title=%q", v.Title)
// 		}
// 	case *ast.List:
// 		// fmt.Printf(" (ordered=%v)", v.IsOrdered)
// 	case *ast.Table:
// 		fmt.Printf(" (table)")
// 	case *ast.HTMLBlock:
// 		fmt.Printf(" %q", v.Literal)
// 	case *ast.Paragraph:
// 		fmt.Printf(" (paragraph)")
// 	case *ast.Image:
// 		fmt.Printf(" [src=%s]", v.Destination)
// 	case *ast.Strong:
// 		fmt.Printf(" (strong)")
// 	case *ast.Emph:
// 		fmt.Printf(" (emphasis)")
// 	}
// 	fmt.Println()

// 	for _, child := range node.GetChildren() {
// 		printNode(child, level+1)
// 	}
// }

func getHeadingText(heading ast.Heading) string {
	if len(heading.Children) > 0 {
		if txt, ok := heading.Children[0].(*ast.Text); ok {
			return string(txt.Literal)
		}
	}
	return ""
}

func parseCommands(doc ast.Node) []cmdNode {
	var commands []cmdNode
	var stack []*cmdNode // Track current heading hierarchy

	ast.WalkFunc(doc, func(node ast.Node, entering bool) ast.WalkStatus {
		if !entering {
			return ast.GoToNext
		}

		switch v := node.(type) {
		case *ast.Heading:
			cmd := cmdNode{}
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
				parent.Children = append(parent.Children, cmd)
				stack = append(stack, &parent.Children[len(parent.Children)-1])
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

func printCommands(cmds []cmdNode, level int) {
	// indent := strings.Repeat("  ", level)
	maxHeaderLen := 0
	var headingMap = make(map[int]string, 0)
	for _, cmd := range cmds {
		if cmd.CodeBlocks != nil {
			if cmd.Heading.Level == 1 {
				// Skip level 1 headers
				printCommands(cmd.Children, level+1)
				break
			}
			heading := getHeadingText(cmd.Heading)
			
			headingMap[cmd.Heading.Level] = heading
			if value, ok := headingMap[2]; ok {
				fmt.Printf("mapping[%d] is %s\n", 2, value)
			} else {
				fmt.Printf("mapping[%d] is false\n", 2)
			}

			fmt.Printf("%d %s\n", cmd.Heading.Level, heading)

			for _, block := range cmd.CodeBlocks {
				fmt.Printf("  ```%s\n", block.Info)
				// fmt.Printf(" [%s]: \n", block.Info)
				// fmt.Printf("%s  Code[%s]: %s\n", indent, block.Info, string(block.Literal))
				// fmt.Printf("%s", strings.Join(append(strings.Split(string(block.Literal), "\n"), "    "), "\n"))
				lines := strings.Split(string(block.Literal), "\n")
				currentHeaderLen := len(heading)
				if currentHeaderLen > maxHeaderLen {
					maxHeaderLen = currentHeaderLen
				}
				for index, line := range lines {
					if index == len(lines)-1 {
						fmt.Printf("  %s", line)
					} else {
						fmt.Printf("  %s\n", line)
					}
				}
				fmt.Printf("```\n")
			}
		}
		printCommands(cmd.Children, level+1)
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

func executeCodeBlock(block ast.CodeBlock, args []string) error {
	info := string(block.Info) // Convert []byte to string
	// fmt.Printf("Executing code block [%s] with args %v:\n%s\n",
	// 	info, args, string(block.Literal))

	var cmdName string
	var insertArgs []string

	switch info { // Use the converted string for comparison
	case "sh":
		cmdName = "sh"
		insertArgs = []string{"-eu", "-s"}
	case "shell":
		cmdName = "sh"
		insertArgs = []string{"-eu", "-s"}
	case "bash":
		cmdName = "bash"
		insertArgs = []string{"-eu", "-s"}
	case "js":
		cmdName = "node"
		insertArgs = []string{"-"}
	case "javascript":
		cmdName = "node"
		insertArgs = []string{"-"}
	case "py":
		cmdName = "python"
		insertArgs = []string{"-"}
	case "python":
		cmdName = "python"
		insertArgs = []string{"-"}
	case "zig":
		cmdName = "zig"
	default:
		return fmt.Errorf("unsupported code block type: %s", info)
	}

	args = append(insertArgs, args...)

	cmd := exec.Command(cmdName, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.Stdin = strings.NewReader(string(block.Literal))
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("error executing command: %w", err)
	}

	return nil
}

func findAndExecuteNestedCommand(cmds []cmdNode, path []string, args []string, currentDepth int) bool {
	if currentDepth >= len(path) {
		return false
	}

	targetHeading := path[currentDepth]
	for _, cmd := range cmds {
		// Skip level 1 headers and only process level 2+ headers
		if cmd.Heading.Level == 1 {
			// Search through level 1's subcommands directly
			if findAndExecuteNestedCommand(cmd.Children, path, args, currentDepth) {
				return true
			}
			continue
		}

		heading := getHeadingText(cmd.Heading)
		if strings.EqualFold(heading, targetHeading) {
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
			if findAndExecuteNestedCommand(cmd.Children, path, args, currentDepth+1) {
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

	if len(args) < 1 {
		fmt.Println("Usage: markdown-build [-f <input-file>] <heading-path...> [-- <args...>]")
		printCommands(commands, 0)
		// printNode(doc, 0)
		return
	}

	if !findAndExecuteNestedCommand(commands, headingPath, codeArgs, 0) {
		fmt.Printf("Command path '%s' not found\n", strings.Join(headingPath, " > "))
		return
	}
}
