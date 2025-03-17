# tinyemu

| key         | value      |
| ----------- | ---------- |
| PKG_VERSION | 2019-12-21 |

```sh
./main pkgs/tinyemu/build.md
```

## Configure

```sh
if [ ! -f ${SRCS_DIR}/tinyemu-${PKG_VERSION}.tar.gz ]; then
    wget -P ${SRCS_DIR} http://bellard.org/tinyemu/tinyemu-${PKG_VERSION}.tar.gz
fi

gzip -d <${SRCS_DIR}/tinyemu-${PKG_VERSION}.tar.gz | tar -C ${BUILD_DIR} -x
```

```sh
cd ${BUILD_DIR}/tinyemu-${PKG_VERSION}
ls
```
