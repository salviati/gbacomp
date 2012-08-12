/*
   Copyright (c) Utkan Güngördü <utkan@freeconsole.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of

   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the

   GNU General Public License for more details


   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

package gbacomp

//#include "cprs.h"
//#include <string.h>
import "C"

import (
	"bytes"
	"errors"
	"io"
	"io/ioutil"
	"unsafe"
)

type Method int

const (
	RLE  Method = 0x30
	LZ77 Method = 0x10
	//Huffman4 Method = 0x24
	Huffman8 Method = 0x28
)

func (m Method) String() string {
	switch m {
	/*case Huffman4:
	return "Huffman4"*/
	case Huffman8:
		return "Huffman8"
	case RLE:
		return "RLE"
	case LZ77:
		return "LZ77"
	}
	return ""
}

var (
	UnexpectedError = errors.New("Unexpected error")
	UnknownMethod   = errors.New("Unexpected method")
)

func exec(compress bool, method Method, data []byte) ([]byte, error) {
	src := new(C.RECORD)
	src.width = 1
	src.height = C.int(len(data))
	src.data = (*C.uchar)(unsafe.Pointer(&data[0]))
	dst := new(C.RECORD)

	switch method {
	/*case Huffman4:
	if compress {
		C.huffman_encode(dst, src, 4)
	} else {
		C.huffman_decode(dst, src)
	}*/
	case Huffman8:
		if compress {
			C.huffman_encode(dst, src, 8)
		} else {
			C.huffman_decode(dst, src)
		}
	case RLE:
		if compress {
			C.rle8gba_compress(dst, src)
		} else {
			C.rle8gba_decompress(dst, src)
		}
	case LZ77:
		if compress {
			C.lz77gba_compress(dst, src)
		} else {
			C.lz77gba_decompress(dst, src)
		}
	default:
		return []byte{}, UnknownMethod
	}
	defer C.free(unsafe.Pointer(dst.data))

	n := dst.width * dst.height
	if n == 0 {
		return []byte{}, UnexpectedError
	}

	output := make([]byte, n)
	C.memcpy(unsafe.Pointer(&output[0]), unsafe.Pointer(dst.data), C.size_t(n))

	return output, nil
}

func Decompress(data []byte) (decompressed []byte, err error) {
	return exec(false, Method(data[0]), data)
}

// Compresses data using a given method.
func Compress(method Method, data []byte) (compressed []byte, err error) {
	return exec(true, method, data)
}

func NewDecompresser(r io.Reader) (io.Reader, error) {
	read, err := ioutil.ReadAll(r)
	if err != nil {
		return nil, err
	}
	decompressed, err := Decompress(read)
	if err != nil {
		return nil, err
	}
	return bytes.NewReader(decompressed), nil
}

func NewCompressor(w io.Writer, m Method) io.WriteCloser {
	return &Compresser{bytes.NewBuffer([]byte{}), m, w}
}

type Compresser struct {
	*bytes.Buffer
	m Method
	w io.Writer
}

// The data will be compressed & flushed during Close.
func (c *Compresser) Close() error {
	compressed, err := Compress(c.m, []byte(c.String()))
	if err != nil {
		return err
	}
	_, err = c.w.Write(compressed)
	if err != nil {
		return err
	}

	return nil
}
