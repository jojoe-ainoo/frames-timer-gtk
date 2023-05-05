This is a C project using the `ffmpeg`, `cairo` and `gtk` graphics library to build an application

## Project Brief

This project uses ffmpeg libraries to extract 10 frames from a video and convert them to rgb format. Then uses cairo and gtk to load the frames onto a window using a timer.

## Contribution

The application was built by:

```
- Emmanuel Ainoo
- Ayomide Oduba
```

## Requirements

- [`gcc C program compiler`](https://gcc.gnu.org)
- [`ffmpeg`]([https://ffmpeg.org)]
- [`gtk`]([https://ffmpeg.org)]
- `cairo`

## Usage

Compile with gcc & gtk3 flags on terminal:

```shell
gtkffmpeg A5 A5.c
```

gtkffmpeg is an alias that has all the libraries required

Run File:

```shell
./A5 sample.mpg
```

Open A3 directory to locate the 10 frames
