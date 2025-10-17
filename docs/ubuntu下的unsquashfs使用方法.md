# ubuntu下的unsquashfs使用方法

--------

## 一、安装

sudo apt update我就省啦嘻嘻~~

```shell
sudo apt install squashfs-tools -y
```

然后验证安装成功了没有

```shell
unsquashfs -h
mksquashfs -h
```

--------

## 二、打开squashfs文件

先验证一下：

```shell
file your_image.img
```

如果看到类似文本：

```
your_image.img: Squashfs filesystem, little-endian, version 4.0, 16.0 MB, ...
```

那就是Squashfs文件了。

#### **解包squashfs 镜像**

```
unsquashfs your_image.img
```

默认会在命令行路径生成目录：squashfs-root/

你可以指定目录名：

```
unsquashfs -d my_rootfs your_image.img
```

你就可以在命令行路径下的squashfs-root处的编辑squashfs 镜像文件啦 nya\~\~

## 三、打包

~~*我是坚决不会教你打包的啦*~~

~挨打了~，先查看原始镜像信息吧

```sehll
unsquashfs -s your_image.img
```

🌰栗子：

```
Found a valid SQUASHFS 4:0 superblock.
Compression: xz
Block size: 131072
Filesystem size: 16.0 MB
...
```

📌 记下这些参数：

* Compression: xz / gzip / lzo
* Block size: 131072（通常 128K）
* 是否启用 -no-xattrs、-no-fragments、-no-duplicates

*~命~令~模~板*

```shell
mksquashfs squashfs-root new_image.img \
    -comp <COMPRESSION> \
    -b <BLOCK_SIZE> \
    -noappend \
    -no-xattrs \
    -no-fragments \
    -no-duplicates
```

🌰栗子：

```
mksquashfs squashfs-root new_rootfs.img \
    -comp xz \
    -b 131072 \
    -noappend \
    -no-xattrs \
    -no-fragments \
    -no-duplicates
```

> ⚠️ 如果不加 `-noappend`，可能会导致新镜像无法挂载！ 
