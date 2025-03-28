# Demo

The basic idea is make markdown work like makemenuconfig

| key    | value |
| ------ | ----- |
| output | main  |

Test with README.md

1. find the target markdown file in current dir in the name of README.md case ignored, if not found, search upper dir, or direct a file with -f or --file option
2. run Tag as subcommand, following arguments as positional arguments.

```sh
go run main.go
```

```sh
go build -ldflags='-w -s'
du -ahd0 mdrun
```

```sh
hyperfine "./mdrun"
```

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

## Build

```sh
go build -ldflags="-w -s" -o main main.go
```

## Test

```sh
echo Test
```

### x

```sh
echo x
```

### y

```sh
echo y
```

#### z

```js
console.log(1);
```
