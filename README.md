![GitHub release (latest by date)](https://img.shields.io/github/v/release/Text-extend-tools/fictionup)
![GitHub Release Date](https://img.shields.io/github/release-date/Text-extend-tools/fictionup)
![GitHub repo size](https://img.shields.io/github/repo-size/Text-extend-tools/fictionup)
![GitHub all releases](https://img.shields.io/github/downloads/Text-extend-tools/fictionup/total)
![GitHub](https://img.shields.io/github/license/Text-extend-tools/fictionup)

# fictionup

## Description

fictionup is a command line markdown to FB2 converter. It supports a limited set of FB2 meta information tags, which are required to produce valid documents.

FB2 meta information must be provided using YAML data blocks in a markdown document or by '-m' command line switch. YAML data block starts with the line containing '---' (exactly 3 minus signs) and ends with the similarly formatted line or with '...' (exactly 3 dots).

This converter can compress the resulting file into the .fb2.zip archive.

## Meta information example

```
Title: The Book
Author: A. Writer
Year: 2019
Annotation: This book is awesome!
Cover: cover.jpg
Series: [The Complete Collection, 1]
fb2-author: B. Converter
fb2-id: doc-002
fb2-version: 1.0
fb2-date: 2019
```

## Document examples

[Markdown example](./examples/fictionup-example-en.md) [Converted FB2](./examples/fictionup-example-en.fb2)

---

http://cdslow.org.ru/fictionup
