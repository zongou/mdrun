package main

import (
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"strings"

	"github.com/GoToUse/treeprint"
	"github.com/fatih/color"
	"github.com/gomarkdown/markdown/ast"
	"github.com/gomarkdown/markdown/parser"
)

var programName string = path.Base(os.Args[0])

// Create a map for language configurations
var languageConfigs = map[string]languageConfig{
	"awk":        {"awk", []string{"$CODE"}},
	"sh":         {"sh", []string{"-euc", "$CODE", "--"}},
	"bash":       {"bash", []string{"-euc", "$CODE", "--"}},
	"zsh":        {"zsh", []string{"-euc", "$CODE", "--"}},
	"fish":       {"fish", []string{"-euc", "$CODE", "--"}},
	"dash":       {"dash", []string{"-euc", "$CODE", "--"}},
	"ksh":        {"ksh", []string{"-euc", "$CODE", "--"}},
	"ash":        {"ash", []string{"-euc", "$CODE", "--"}},
	"shell":      {"sh", []string{"-euc", "$CODE", "--"}},
	"js":         {"node", []string{"-e", "$CODE"}},
	"javascript": {"node", []string{"-e", "$CODE"}},
	"py":         {"python", []string{"-c", "$CODE"}},
	"python":     {"python", []string{"-c", "$CODE"}},
	"rb":         {"ruby", []string{"-e", "$CODE"}},
	"ruby":       {"ruby", []string{"-e", "$CODE"}},
	"php":        {"php", []string{"-r", "$CODE"}},
	"cmd":        {"cmd.exe", []string{"/c", "$CODE"}},
	"batch":      {"cmd.exe", []string{"/c", "$CODE"}},
	"powershell": {"powershell.exe", []string{"-c", "$CODE"}},
}

type cmdNode struct {
	Heading     ast.Heading
	CodeBlocks  []ast.CodeBlock
	Children    []cmdNode
	Env         map[string]string
	Parent      *cmdNode
	Description string
}

// findDoc searches for a {programName}.md file, then .{programName}.md, then README.md in the current or parent directories.
func findDoc() (string, error) {
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
			// Check for "{programName}.md" ignoring case
			if !file.IsDir() && strings.EqualFold(file.Name(), programName+".md") {
				return filepath.Join(dir, file.Name()), nil
			}
			// Check for ".{porgramName}.md" ignoring case
			if !file.IsDir() && strings.EqualFold(file.Name(), "."+programName+".md") {
				return filepath.Join(dir, file.Name()), nil
			}
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

	return "", fmt.Errorf("cr.md, .cr.md, or README.md not found")
}

func getHeadingText(heading ast.Heading) string {
	if len(heading.Children) > 0 {
		if txt, ok := heading.Children[0].(*ast.Text); ok {
			return string(txt.Literal)
		}
	}
	return ""
}

func parseDoc(doc ast.Node) []cmdNode {
	var commands []cmdNode
	var stack []*cmdNode // Track current heading hierarchy

	ast.WalkFunc(doc, func(node ast.Node, entering bool) ast.WalkStatus {
		if !entering {
			return ast.GoToNext
		}

		switch v := node.(type) {
		case *ast.Heading:
			cmdNode := cmdNode{Heading: *v}

			// Pop stack until we find appropriate parent level
			for len(stack) > 0 && stack[len(stack)-1].Heading.Level >= v.Level {
				stack = stack[:len(stack)-1]
			}

			if len(stack) == 0 {
				commands = append(commands, cmdNode)
				stack = append(stack, &commands[len(commands)-1])
			} else {
				parent := stack[len(stack)-1]
				parent.Children = append(parent.Children, cmdNode)
				current := &parent.Children[len(parent.Children)-1]
				current.Parent = parent
				stack = append(stack, current)
			}

		case *ast.Paragraph:
			if len(stack) > 0 {
				current := stack[len(stack)-1]
				if current.Description == "" && len(current.CodeBlocks) == 0 {
					var description strings.Builder
					ast.WalkFunc(node, func(child ast.Node, entering bool) ast.WalkStatus {
						if !entering {
							return ast.GoToNext
						}

						switch v := child.(type) {
						case *ast.Text:
							description.WriteString(strings.ReplaceAll(string(v.Literal), "\n", " "))
						case *ast.Hardbreak:
							description.WriteString("\n")
						}

						return ast.GoToNext
					})
					current.Description = description.String()
				}
			}

		case *ast.CodeBlock:
			if len(stack) > 0 {
				current := stack[len(stack)-1]
				if _, exists := languageConfigs[string(v.Info)]; exists {
					current.CodeBlocks = append(current.CodeBlocks, *v)
				}
			}

		case *ast.Table:
			if len(stack) > 0 {
				current := stack[len(stack)-1]
				if current.Env == nil {
					current.Env = make(map[string]string)
				}
				ast.WalkFunc(v, func(child ast.Node, entering bool) ast.WalkStatus {
					if !entering {
						return ast.GoToNext
					}

					switch v := child.(type) {
					case *ast.TableRow:
						if len(v.Children) >= 2 {
							keyNode, valNode := v.Children[0], v.Children[1]
							if keyText, ok := keyNode.GetChildren()[0].(*ast.Text); ok {
								if valText, ok := valNode.GetChildren()[0].(*ast.Text); ok {
									current.Env[string(keyText.Literal)] = string(valText.Literal)
								}
							}
						}
					}

					return ast.GoToNext
				})
			}
		}

		return ast.GoToNext
	})

	return commands
}

// Define a struct for language configuration
type languageConfig struct {
	cmdName    string
	prefixArgs []string
}

func execCmdNode(cmdNode cmdNode, args []string) error {
	for _, codeBlock := range cmdNode.CodeBlocks {
		info := string(codeBlock.Info) // Convert []byte to string

		// Lookup language configuration
		config, exists := languageConfigs[info]
		if !exists {
			return fmt.Errorf("unsupported code block type: %s", info)
		}

		// Replace $CODE placeholder with the actual code block
		prefixArgs := make([]string, len(config.prefixArgs))
		for i, arg := range config.prefixArgs {
			prefixArgs[i] = strings.Replace(arg, "$CODE", string(codeBlock.Literal), 1)
		}

		cmdArgs := append(prefixArgs, args...)

		// Merge environment variables ensuring current node's variables take precedence
		envMap := make(map[string]string)
		for parent := cmdNode.Parent; parent != nil; parent = parent.Parent {
			for key, value := range parent.Env {
				if _, exists := envMap[key]; !exists {
					envMap[key] = value
				}
			}
		}
		for key, value := range cmdNode.Env {
			envMap[key] = value
		}

		// Convert map to slice of "key=value" strings
		var cmdEnv []string
		for key, value := range envMap {
			cmdEnv = append(cmdEnv, key+"="+value)
		}
		cmdEnv = append(os.Environ(), cmdEnv...)

		// Execute the command
		cmd := exec.Command(config.cmdName, cmdArgs...)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		cmd.Stdin = os.Stdin
		cmd.Env = cmdEnv
		if err := cmd.Run(); err != nil {
			return fmt.Errorf("error executing command %s with args %v: %w", config.cmdName, cmdArgs, err)
		}
	}

	return nil
}

func findAndExecuteNestedCommand(nodes []cmdNode, path []string, args []string, currentDepth int) bool {
	if currentDepth >= len(path) {
		return false
	}

	targetHeading := path[currentDepth]
	for _, node := range nodes {
		// Skip level 1 headers and only process level 2+ headers
		if node.Heading.Level == 1 {
			// Search through level 1's subcommands directly
			if findAndExecuteNestedCommand(node.Children, path, args, currentDepth) {
				return true
			}
			continue
		}

		heading := getHeadingText(node.Heading)
		if strings.EqualFold(heading, targetHeading) {
			if currentDepth == len(path)-1 {
				execCmdNode(node, args)
				return true
			}
			// Continue searching in subcommands
			if findAndExecuteNestedCommand(node.Children, path, args, currentDepth+1) {
				return true
			}
		}
	}
	return false
}

func showCommands(cmdNodes []cmdNode, verbose bool) {
	if cmdNodes != nil {
		var treeView func(cmdNode cmdNode, level int, branch treeprint.Tree)
		treeView = func(cmdNode cmdNode, level int, branch treeprint.Tree) {
			for _, child := range cmdNode.Children {
				if len(child.CodeBlocks) > 0 || len(child.Children) > 0 {
					branch := branch.AddBranch(getHeadingText(child.Heading))

					treeView(child, level+1, branch)
				}
			}
		}

		var treeViewWithDescription func(cmdNode cmdNode, level int, branch treeprint.Tree, maxLineRuneLen int)
		treeViewWithDescription = func(cmdNode cmdNode, level int, branch treeprint.Tree, maxLineRuneLen int) {
			for _, child := range cmdNode.Children {
				if len(child.CodeBlocks) > 0 || len(child.Children) > 0 {
					var sb strings.Builder

					heading := getHeadingText(child.Heading)
					headingLowerCased := strings.ToLower(heading)
					sb.WriteString(color.GreenString(headingLowerCased))

					discription := child.Description

					if verbose {
						for k, v := range child.Env {
							envPrettied := color.BlueString(k + "=" + v)
							if discription == "" {
								discription = envPrettied
							} else {
								discription = discription + "\n" + envPrettied
							}
						}
						for _, codeBlock := range child.CodeBlocks {
							codeBlockTrimmed := strings.TrimSuffix(string(codeBlock.Literal), "\n")
							codeBlockPrettied := "```" + string(codeBlock.Info) + "\n" + codeBlockTrimmed + "\n```"
							if discription == "" {
								discription = codeBlockPrettied
							} else {
								discription = discription + "\n" + codeBlockPrettied
							}
						}
					}

					linesOfDescription := strings.Split(discription, "\n")
					for i, line := range linesOfDescription {
						divider := "  "
						if i == 0 {
							sb.WriteString(divider)
							sb.WriteString(strings.Repeat(" ", maxLineRuneLen-(level+1)*4-len([]rune(heading))))
						} else {
							sb.WriteString("\n")
							sb.WriteString(divider)
							sb.WriteString(strings.Repeat(" ", maxLineRuneLen-(level+1)*4))
						}
						sb.WriteString(line)
					}

					branch := branch.AddBranch(sb.String())

					treeViewWithDescription(child, level+1, branch, maxLineRuneLen)
				}
			}
		}

		for _, cmdNode := range cmdNodes {
			tree := treeprint.New()
			treeView(cmdNode, 0, tree)
			lines := strings.Split(tree.String(), "\n")
			// Get maxLine length
			maxLineRuneLen := 0
			for _, line := range lines {
				runeLength := len([]rune(line)) // Returns the number of characters (runes)
				if runeLength > maxLineRuneLen {
					maxLineRuneLen = runeLength
				}
			}

			// fmt.Print(tree.String())
			// fmt.Printf("longestHeadingPath: %v\n", longestHeadingPath)

			treeWithDescription := treeprint.New()
			treeWithDescription.SetValue(getHeadingText(cmdNode.Heading))
			treeViewWithDescription(cmdNode, 0, treeWithDescription, maxLineRuneLen)
			fmt.Println(treeWithDescription.String())
		}

	}
}

func showHelp() {
	const indention = "    "

	fmt.Printf("%s\n\n", "Run markdown codeblocks by its heading.")
	fmt.Printf("%s\n", color.YellowString("USAGE:"))
	fmt.Printf("%s%s [--file FILE] <heading...> [-- <args...>]\n", indention, programName)
	fmt.Println()

	fmt.Printf("%s\n", color.YellowString("FLAGS:"))
	fmt.Printf("%s-h, --help        Show this help\n", indention)
	fmt.Printf("%s-v, --verbose     Print more information\n", indention)
	fmt.Println()

	fmt.Printf("%s\n", color.YellowString("OPTIONS:"))
	fmt.Printf("%s-f, --file        MarkDown file to use\n", indention)
	fmt.Println()
	// fmt.Println("Use 'md [command] --help' for more information about a command.")
}

func main() {
	var config struct {
		help    bool
		verbose bool
		file    string
	}

	flag.BoolVar(&config.help, "h", false, "show this help")
	flag.BoolVar(&config.help, "help", false, "show this help")
	flag.BoolVar(&config.verbose, "v", false, "enable verbose mode")
	flag.BoolVar(&config.verbose, "verbose", false, "enable verbose mode")
	flag.StringVar(&config.file, "f", "", "specify the input file")
	flag.StringVar(&config.file, "file", "", "specify the input file")

	// Customize help message
	flag.Usage = func() {
		showHelp()
	}

	flag.Parse()

	var inputFile string
	switch {
	case config.file != "":
		inputFile = config.file
	default:
		var err error
		inputFile, err = findDoc()
		if err != nil {
			fmt.Printf("Error finding document: %v\n", err)
			return
		}
	}

	content, err := os.ReadFile(inputFile)
	if err != nil {
		fmt.Printf("Error reading file: %v\n", err)
		return
	}

	os.Setenv("MD_EXE", os.Args[0])
	os.Setenv("MD_FILE", inputFile)

	extensions := parser.CommonExtensions | parser.AutoHeadingIDs | parser.NoEmptyLineBeforeBlock
	p := parser.NewWithExtensions(extensions)
	doc := p.Parse(content)

	cmdNodes := parseDoc(doc)

	args := flag.Args()

	// Split args into heading path and code block args
	var headingPath []string
	var subCmdArgs []string
	for i, arg := range args {
		if arg == "--" {
			headingPath = args[:i]
			subCmdArgs = args[i+1:]
			break
		}
	}
	if len(subCmdArgs) == 0 { // No "--" found
		headingPath = args
	}

	if config.help {
		showHelp()
		return
	}

	if len(headingPath) == 0 {
		showCommands(cmdNodes, config.verbose)
		return
	}

	if !findAndExecuteNestedCommand(cmdNodes, headingPath, subCmdArgs, 0) {
		fmt.Printf("Command path '%s' not found\n", strings.Join(headingPath, " > "))
		return
	}
}
