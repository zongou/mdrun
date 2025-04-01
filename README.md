# Run MarkDown Codeblocks

Run MarkDown Codeblocks by its heading.  
It searches **README.md** in the current directory and in the parent directories, case ignored.  
You can refer the MarkDown file with option `-f` or `--file`.  
For more information, run `mdrun --help`.

For example:

```sh
./mdrun test sh -- foo bar
```

Features:

- scoped env

Prefixed env

- MDRUN
- MDRUN_FILE

Custom env

| key        | value  |
| ---------- | ------ |
| scope_root | foo    |
| scope      | global |

The basic idea is make markdown work like makemenuconfig

Test with README.md

1. find the target markdown file in current dir in the name of README.md case ignored, if not found, search upper dir, or direct a file with -f or --file option
2. run Tag as subcommand, following arguments as positional arguments.

## To-Does

- [ ] depends
- [ ] target toolchain

## Configure

tidy
go run tidy  
another line

```sh
go mod tidy
```

## Build

Build mdrun

```sh
go build -ldflags="-w -s"
```

## benchmark

```sh
go run . build
hyperfine "${MDRUN} test env"
```

## Test

Test mdrun

| key        | value |
| ---------- | ----- |
| scope_test | bar   |
| scope      | test  |

```sh
go run . build
${MDRUN} test env
${MDRUN} test env subenv
${MDRUN} test args
${MDRUN} test multiple
${MDRUN} test stdin
${MDRUN} test awk
```

### env

test scoped env

| key       | value |
| --------- | ----- |
| scope_env | 123   |
| scope     | env   |

```sh
for env in \
    MDRUN \
    MDRUN_FILE \
    scope_root \
    scope_test \
    scope_env \
    scope; do

    eval echo "env ${env}=\${${env}}"
done
```

#### subenv

xx

| key   | value  |
| ----- | ------ |
| scope | subenv |

```sh
echo "subenv scope=${scope}"
```

### sh

```sh
echo "shellscript with arguments: $*"
```

### js

```js
console.log(`nodejs with arguments: ${process.argv}`);
```

### py

```python
import sys

print("python with arguments: %s" %(sys.argv))
```

### awk_example

```awk
{print $1}
```

### awk

```sh
size=$(du -ahd0 ${MDRUN} | ${MDRUN} test awk_example)
echo "mdrun size: $size"
```

### args

```sh
${MDRUN} test sh -- a b c
${MDRUN} test js -- a b c
${MDRUN} test py -- a b c
```

### multiple

```sh
echo codeblock 1 in multiple
```

```sh
echo codeblock 2 in multiple
```

### stdin_example

longtest

```sh
echo "stdin: $(cat)"
```

### stdin

```sh
echo Hello | ${MDRUN} test stdin_example
```

## busybox

| key    | value                   |
| ------ | ----------------------- |
| TARGET | aarch64-linux-android24 |

```sh
${MDRUN} -f ./pkg.md build -- busybox
```

## reset

```sh
git reset --hard $(git rev-list --max-parents=0 HEAD)
git pull
```

## tools

```sh
mask --maskfile ./README.md --help
xc -H "Test" --no-tty
```

---

Inspired by [mask](https://github.com/jacobdeichert/mask) and [xc](https://github.com/joerdav/xc).
