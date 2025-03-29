# Demo

The basic idea is make markdown work like makemenuconfig

| key    | value |
| ------ | ----- |
| output | main  |

| command   | description |
| --------- | ----------- |
| configure | configure   |

Test with README.md

1. find the target markdown file in current dir in the name of README.md case ignored, if not found, search upper dir, or direct a file with -f or --file option
2. run Tag as subcommand, following arguments as positional arguments.

## To-Does

- [ ] depends
- [ ] target toolchain

## Configure

Print custom and built-in env

```sh
echo output=${output}
echo NCPU=${NCPU}
echo SRCS_DIR=${SRCS_DIR}
echo BUILD_DIR=${BUILD_DIR}
echo OUTPUT_DIR=${OUTPUT_DIR}
echo PKG_DIR=${PKG_DIR}
```

## sh

```sh
echo "shellscript with arguments: $*"
```

## js

```js
console.log(`nodejs with arguments: ${process.argv.slice(2)}`);
```

## py

```python
import sys

print("python with arguments: %s" %(sys.argv))
```

## Build

```sh
go build -ldflags="-w -s"
du -ahd0 ./mdrun
```

## Test2

```sh
go run main.go Build
./mdrun
./mdrun configure
./mdrun sh -- a b c
./mdrun js -- a b c
./mdrun py -- a b c
```

## tools

```sh
mask --maskfile ./README.md --help
xc --no-tty
```

### tools1

```sh
echo example1
```

```sh
echo example2
```
