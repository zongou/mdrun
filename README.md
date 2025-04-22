# CR (Codeblock Runner)

Run markdown codeblocks by its heading.  
It searches(case ignored) for a cr.md file, then .cr.md, then README.md in the current or parent directories.  
You can refer the markdown file to use with option `-f` or `--file`.  
For more information, run with option `--help`.

For example:

```sh
./cr test sh -- foo bar
```

Features:

- scoped env

Prefixed env

- MD_EXE
- MD_FILE

Custom env

| key          | value |
| ------------ | ----- |
| HEADING_ROOT | root  |
| HEADING      | root  |
| CGO_ENABLED  | 0     |
| PROGRAM      | cr    |

The basic idea is make markdown work like makemenuconfig

Test with README.md

1. find the target markdown file in current dir in the name of README.md case ignored, if not found, search upper dir, or direct a file with -f or --file option
2. run Tag as subcommand, following arguments as positional arguments.

## To-Does

- [ ] depends
- [ ] target toolchain

## Test non-configured languages

```txt
Show not be displayed
```

## Configure

Run go mod tidy

```sh
go mod tidy
```

## Build

Build this program

```sh
# go build -ldflags="-w -s"
zig cc --target=$(uname -m)-linux-musl -o "${PROGRAM}" -static -s -Os main.c
file "${PROGRAM}"
du -ahd0 "${PROGRAM}"
```

## Install

Install this program

```sh
PROG=$(basename "${MD_EXE}")
if command -v sudo >/dev/null; then
    sudo install "${PROG}" "/usr/local/bin/${PROG}"
elif test "${PREFIX+1}"; then
    install "${PROG}" "${PREFIX}/bin/${PROG}"
fi
```

## Uninstall

Uninstall this program

```sh
PROG=$(basename "${MD_EXE}")
if command -v sudo >/dev/null; then
    sudo rm $(command -v "${PROG}")
elif test "${PREFIX+1}"; then
    rm $(command -v "${PROG}")
fi
```

## Benchmark

Benchmark this program

```sh
hyperfine "${MD_EXE} test env" "$@"
```

## Test

Test this program

| key     | value |
| ------- | ----- |
| HEADING | Test  |

```sh
${MD_EXE} test env
${MD_EXE} test env sub
${MD_EXE} test args
${MD_EXE} test multiple
echo Hello | ${MD_EXE} test stdin
echo "cr file size: $(du -ahd0 ${MD_EXE} | ${MD_EXE} test awk)"
```

### env

Test scoped env

| key     | value |
| ------- | ----- |
| HEADING | env   |

```sh
for env in \
    MD_EXE \
    MD_FILE \
    HEADING \
    HEADING_ROOT; do

    eval echo "env ${env}=\${${env}}"
done
```

#### sub

Test scoped env (heading +1)

| key     | value |
| ------- | ----- |
| HEADING | sub   |

```sh
echo "sub HEADING=${HEADING}"
```

### sh

Test shellscript

```sh
echo "shellscript with arguments: $*"
```

### js

Test javascript

```js
console.log(`nodejs with arguments: ${process.argv}`);
```

### py

Test python

```python
import sys

print("python with arguments: %s" %(sys.argv))
```

### awk

Print first column in awk

```awk
{print $1}
```

### args

Test program with arguments

```sh
${MD_EXE} test sh -- a b c
${MD_EXE} test js -- a b c
${MD_EXE} test py -- a b c
```

### multiple

Test multiple codeblocks

```sh
echo codeblock 1 in multiple
```

```sh
echo codeblock 2 in multiple
```

### stdin

Read stdin in shell

```sh
echo "stdin: $(cat)"
```

## Reset

Reset to the initial commit

```sh
git reset --hard $(git rev-list --max-parents=0 HEAD)
git pull
```

---

Inspired by [mask](https://github.com/jacobdeichert/mask) and [xc](https://github.com/joerdav/xc).

# Two

## ls

```sh
ls
```
