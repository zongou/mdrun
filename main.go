package main

import (
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/GoToUse/treeprint"
	"github.com/fatih/color"
	"github.com/gomarkdown/markdown/ast"
	"github.com/gomarkdown/markdown/parser"
	"github.com/posener/complete/v2"
	"github.com/posener/complete/v2/install"
	"github.com/posener/complete/v2/predict"
)

type Node struct {
	Heading     ast.Heading
	CodeBlocks  []ast.CodeBlock
	Children    []Node
	Env         map[string]string
	Parent      *Node
	Description string
}

func getHeadingText(heading ast.Heading) string {
	if len(heading.Children) > 0 {
		if txt, ok := heading.Children[0].(*ast.Text); ok {
			return string(txt.Literal)
		}
	}
	return ""
}

func parseCommands(doc ast.Node) []Node {
	var commands []Node
	var stack []*Node // Track current heading hierarchy

	ast.WalkFunc(doc, func(node ast.Node, entering bool) ast.WalkStatus {
		if !entering {
			return ast.GoToNext
		}

		switch v := node.(type) {
		case *ast.Heading:
			cmdNode := Node{}
			cmdNode.Heading = *v

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

				// Only consider this paragraph if it directly follows a heading and precedes a code block
				if current.Description == "" && len(current.CodeBlocks) == 0 {
					var description strings.Builder
					ast.WalkFunc(node, func(node ast.Node, entering bool) ast.WalkStatus {
						if !entering {
							return ast.GoToNext
						}

						switch v := node.(type) {
						case *ast.Text:
							// Replace \n with space before appending
							text := strings.ReplaceAll(string(v.Literal), "\n", " ")
							description.WriteString(text)
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
				current.CodeBlocks = append(current.CodeBlocks, *v)
			}
		case *ast.Table:
			if len(stack) > 0 {
				current := stack[len(stack)-1]
				if current.Env == nil {
					current.Env = make(map[string]string)
				}

				ast.WalkFunc(node, func(node ast.Node, entering bool) ast.WalkStatus {
					if !entering {
						return ast.GoToNext
					}

					switch v := node.(type) {
					case *ast.TableHeader:
						return ast.SkipChildren
					case *ast.TableRow:
						key := string(v.Children[0].GetChildren()[0].(*ast.Text).Literal)
						val := string(v.Children[1].GetChildren()[0].(*ast.Text).Literal)
						current.Env[key] = val
					}

					return ast.GoToNext
				})
			}
		}

		return ast.GoToNext
	})

	return commands
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

func execCmdNode(cmdNode Node, args []string) error {
	for _, codeBlock := range cmdNode.CodeBlocks {
		info := string(codeBlock.Info) // Convert []byte to string
		// fmt.Printf("Executing code block [%s] with args %v:\n%s\n",
		// 	info, args, string(block.Literal))

		var cmdName string
		var prefixArgs []string

		switch info { // Use the converted string for comparison
		case "awk":
			cmdName = "awk"
			prefixArgs = []string{string(codeBlock.Literal)}
		case "sh", "bash", "zsh", "fish", "dash", "ksh", "ash":
			cmdName = info
			prefixArgs = []string{"-c", string(codeBlock.Literal), "--"}
		case "shell":
			cmdName = "sh"
			prefixArgs = []string{"-c", string(codeBlock.Literal), "--"}
		case "js", "javascript":
			cmdName = "node"
			prefixArgs = []string{"-e", string(codeBlock.Literal)}
		case "py", "python":
			cmdName = "python"
			prefixArgs = []string{"-c", string(codeBlock.Literal)}
		case "rb", "ruby":
			cmdName = "ruby"
			prefixArgs = []string{"-e", string(codeBlock.Literal)}
		case "php":
			cmdName = "php"
			prefixArgs = []string{"-r", string(codeBlock.Literal)}
		case "cmd", "batch":
			cmdName = "cmd.exe"
			prefixArgs = []string{"/c", string(codeBlock.Literal)}
		case "powershell":
			cmdName = "powershell.exe"
			prefixArgs = []string{"-c", string(codeBlock.Literal)}
		default:
			return fmt.Errorf("unsupported code block type: %s", info)
		}

		cmdArgs := append(prefixArgs, args...)

		var cmdEnv []string

		for key, value := range cmdNode.Env {
			cmdEnv = append(cmdEnv, key+"="+value)
		}

		for parent := cmdNode.Parent; parent != nil; parent = parent.Parent {
			var parentEnv []string
			for key, value := range parent.Env {
				parentEnv = append(parentEnv, key+"="+value)
			}
			cmdEnv = append(parentEnv, cmdEnv...)
		}

		cmdEnv = append(os.Environ(), cmdEnv...)

		cmd := exec.Command(cmdName, cmdArgs...)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		cmd.Stdin = os.Stdin
		cmd.Env = cmdEnv
		if err := cmd.Run(); err != nil {
			return fmt.Errorf("error executing command: %w", err)
		}
	}

	return nil
}

func findAndExecuteNestedCommand(nodes []Node, path []string, args []string, currentDepth int) bool {
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

func showHelp(cmdNodes []Node, verbose bool) {
	programName := filepath.Base(os.Args[0])
	const indention = "    "

	fmt.Printf("%s\n\n", "Run MarkDown Codeblocks by its heading.")
	fmt.Printf("%s\n", color.YellowString("USAGE:"))
	fmt.Printf("%s%s [-f <file>] <heading...> [-- <args...>]\n", indention, programName)
	fmt.Println()

	fmt.Printf("%s\n", color.YellowString("FLAGS:"))
	fmt.Printf("%s -h, --help\n", indention)
	fmt.Printf("%s -v, --verbose\n", indention)
	fmt.Println()

	fmt.Printf("%s\n", color.YellowString("OPTIONS:"))
	fmt.Printf("%s -f, --file <file>\n", indention)
	fmt.Println()

	if cmdNodes != nil {
		fmt.Printf("%s\n", color.YellowString("HEADING_GUIDE:"))

		var treeView func(cmdNode Node, level int, branch treeprint.Tree)
		treeView = func(cmdNode Node, level int, branch treeprint.Tree) {
			for _, child := range cmdNode.Children {
				if len(child.CodeBlocks) > 0 || len(child.Children) > 0 {
					branch := branch.AddBranch(getHeadingText(child.Heading))

					treeView(child, level+1, branch)
				}
			}
		}

		var treeViewWithDescription func(cmdNode Node, level int, branch treeprint.Tree, maxLineRuneLen int)
		treeViewWithDescription = func(cmdNode Node, level int, branch treeprint.Tree, maxLineRuneLen int) {
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
	// fmt.Println("Use 'md [command] --help' for more information about a command.")
}

func completeSub(nodes []Node) map[string]*complete.Command {
	result := map[string]*complete.Command{}

	for _, node := range nodes {
		if node.Heading.Level > 1 {
			// Ensure getHeadingText does not return an empty string or cause a panic
			headingLowerCased := strings.ToLower(getHeadingText(node.Heading))
			if headingLowerCased != "" { // Avoid adding empty keys to the map
				result[headingLowerCased] = &complete.Command{
					Sub: completeSub(node.Children),
				}
			}
		} else {
			// Append the result of the recursive call to avoid losing data
			subCommands := completeSub(node.Children)
			for k, v := range subCommands {
				result[k] = v
			}
		}
	}

	return result
}

type Config struct {
	FilePath   string
	Verbose    bool
	Args       []string
	complete   bool
	uncomplete bool
}

func main() {
	var config Config
	flag.StringVar(&config.FilePath, "f", "", "Path to the markdown file")
	flag.StringVar(&config.FilePath, "file", "", "Path to the markdown file (same as -f)")

	flag.BoolVar(&config.Verbose, "v", false, "enable verbose mode")
	flag.BoolVar(&config.Verbose, "verbose", false, "enable verbose mode (same as -v)")

	flag.BoolVar(&config.complete, "complete", false, "install shell completion")
	flag.BoolVar(&config.uncomplete, "uncomplete", false, "uninstall shell completion")

	flag.Usage = func() {
		showHelp(nil, config.Verbose) // Assuming false for showInShort
	}

	flag.Parse()

	programName := filepath.Base(os.Args[0])

	if config.complete {
		install.Install(programName)
		return
	}
	if config.uncomplete {
		install.Uninstall(programName)
		return
	}

	var inputFile string
	if config.FilePath != "" {
		inputFile = config.FilePath
	} else {
		var err error
		inputFile, err = findReadme()
		if err != nil {
			fmt.Println("Error:", err)
			fmt.Println("Usage:  [-f <file>]")
			return
		}
	}

	content, err := os.ReadFile(inputFile)
	if err != nil {
		fmt.Printf("Error reading file: %v\n", err)
		return
	}

	os.Setenv("MDRUN", os.Args[0])
	os.Setenv("MDRUN_FILE", inputFile)

	extensions := parser.CommonExtensions | parser.AutoHeadingIDs | parser.NoEmptyLineBeforeBlock
	p := parser.NewWithExtensions(extensions)
	doc := p.Parse(content)

	nodes := parseCommands(doc)

	cmd := &complete.Command{
		Flags: map[string]complete.Predictor{
			"h":          predict.Something,
			"help":       predict.Something,
			"v":          predict.Something,
			"verbose":    predict.Something,
			"f":          predict.Files("*.md"),
			"file":       predict.Files("*.md"),
			"complete":   predict.Something,
			"uncomplete": predict.Something,
		},
		Sub: completeSub(nodes),
	}
	cmd.Complete(programName)

	flag.Usage = func() {
		showHelp(nodes, false) // Assuming false for showInShort
	}

	args := flag.Args()

	// Split args into heading path and code block args
	var headingPath []string
	for i, arg := range args {
		if arg == "--" {
			headingPath = args[:i]
			config.Args = args[i+1:]
			break
		}
	}
	if len(config.Args) == 0 { // No "--" found
		headingPath = args
	}

	flag.Usage = func() {
		showHelp(nodes, config.Verbose) // Assuming false for showInShort
	}

	if len(args) < 1 {
		flag.Usage()
		return
	}

	if !findAndExecuteNestedCommand(nodes, headingPath, config.Args, 0) {
		fmt.Printf("Command path '%s' not found\n", strings.Join(headingPath, " > "))
		return
	}
}
