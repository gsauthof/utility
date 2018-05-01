#!/usr/bin/env python3

# 2018, Georg Sauthoff <mail@gms.tf>
#
# SPDX-License-Identifier: GPL-3.0-or-later

import argparse
import sys

# import PyPDF2 # imported inside merge()
# import pdfrw  # imported inside merge_pdfrw()

def mk_arg_parser():
  p = argparse.ArgumentParser(
        description='Merge two PDF documents',
        epilog='''Merging two PDF documents means that the pages of the
        second document are put on top of the pages of the first one. A
        common use case is the merging of two PDF layers, e.g. a text layer
        (from an OCR software) and and an image layer. This is similar to
        watermarking, although the watermark is usually just a single page
        that is repeatedly merged with each page.

        If one input document has less pages than the other one then the
        remaining pages are copied into the output document, as-is.''')
  p.add_argument('filename1', metavar='INPUT1', nargs=1,
      help='first input PDF file')
  p.add_argument('filename2', metavar='INPUT2', nargs=1,
      help='second input PDF file')
  p.add_argument('ofilename', metavar='OUTPUT', nargs=1,
      help='the merged PDF file')
  p.add_argument('--pdfrw', action='store_true',
      help='Use the pdfrw package instead of PyPDF2')
  return p

def parse_args(*a):
  arg_parser = mk_arg_parser()
  args = arg_parser.parse_args(*a)
  args.filename1 = args.filename1[0]
  args.filename2 = args.filename2[0]
  args.ofilename = args.ofilename[0]
  return args

# cf. https://github.com/tesseract-ocr/tesseract/issues/660#issuecomment-273629726
# e.g. text, images, merged
def merge(filename1, filename2, ofilename):
  import PyPDF2
  with open(filename1, 'rb') as f1, open(filename2, 'rb') as f2:
    # PdfFileReader isn't usable as context-manager
    pdf1, pdf2 = (PyPDF2.PdfFileReader(x) for x in (f1, f2))
    opdf = PyPDF2.PdfFileWriter()
    for page1, page2 in zip(pdf1.pages, pdf2.pages):
      page1.mergePage(page2)
      opdf.addPage(page1)
    n1, n2 = len(pdf1.pages), len(pdf2.pages)
    if n1 != n2:
      for page in (pdf2.pages[n1:] if n1 < n2 else pdf1.pages[n2:]):
        opdf.addPage(page)
    with open(ofilename, 'wb') as g:
      opdf.write(g)

def merge_pdfrw(filename1, filename2, ofilename):
  import pdfrw
  # PdfReader isn't usable as context-manager
  pdf1, pdf2 = (pdfrw.PdfReader(x) for x in (filename1, filename2))
  w = pdfrw.PdfWriter()
  for page1, page2 in zip(pdf1.pages, pdf2.pages):
    m = pdfrw.PageMerge(page1)
    m.add(page2)
    m.render()
    w.addpage(page1)
  # is sufficient if both files contain the same number of pages:
  # w.write(ofilename, pdf1)
  n1, n2 = len(pdf1.pages), len(pdf2.pages)
  if n1 != n2:
    for page in (pdf2.pages[n1:] if n1 < n2 else pdf1.pages[n2:]):
      w.addpage(page)
  w.write(ofilename)

def main():
  args = parse_args()
  if args.pdfrw:
    merge_pdfrw(args.filename1, args.filename2, args.ofilename)
  else:
    merge(args.filename1, args.filename2, args.ofilename)

if __name__ == '__main__':
  sys.exit(main())
