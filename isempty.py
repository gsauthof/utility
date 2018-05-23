#!/usr/bin/env python3

# Detect empty images - e.g. in scanned images
#
# Stand-alone version of the empty-page-detection code
# in https://github.com/gsauthof/adf2pdf/blob/master/adf2pdf.py
#
# 2018, Georg Sauthoff <mail@gms.tf>
# SPDX-License-Identifier: GPL-3.0-or-later

import argparse
import logging
import PIL.Image
import PIL.ImageFilter
import PIL.ImageStat
import sys

log = logging.getLogger(__name__)

def avg_brightness(filename):
    img = PIL.Image.open(filename)
    log.debug('Dimensions (W x H): {}'.format(img.size))
    img = img.convert('L')
    # exclude the margins to ignore punch holes etc.
    img = img.crop((args.margin, args.margin, img.size[0] - args.margin, img.size[1] - args.margin))
    stat = PIL.ImageStat.Stat(img)
    # shifting the mean to deal with empty pages where the
    # reverse page shines through a little
    m = stat.mean[0] - 50
    log.debug('Image avg brightness: {}'.format(m))
    log.debug('Image rms brightness: {}'.format(stat.rms[0]))
    return img, m

def binarize(input_img, thresh):
    # with grayscale, 0 is black and 255 is white (bightest)
    # to simplify the counting we binarize to: 0=white, 1=black
    img = input_img.point(lambda v : v < thresh)
    return img

def erode(input_img):
    img = input_img.filter(PIL.ImageFilter.MinFilter(args.erode))
    return img

def count_black_px(img):
    n = img.size[0] * img.size[1]
    x = sum(img.getdata())
    log.debug('{} of {} pixels are black ({:.2f} %)'.format(x, n, x/n*100))
    return x

# cf. https://dsp.stackexchange.com/a/48837/35404
def is_empty(filename):
    img, thresh = avg_brightness(filename)
    img = binarize(img, thresh)
    count_black_px(img)
    img = erode(img)
    x = count_black_px(img)
    return x < 100



def setup_logging(verbose):
    log_format      = '%(asctime)s - %(levelname)-8s - %(message)s'
    log_date_format = '%Y-%m-%d %H:%M:%S'
    logging.basicConfig(format=log_format, datefmt=log_date_format,
        level=(logging.DEBUG if verbose else logging.INFO))

def parse_args():
    p = argparse.ArgumentParser(
            description='Detect empty images',
            epilog='Exits with status equal 0 if the image is empty and 1 otherwise.')
    p.add_argument('filename', metavar='IMAGE', nargs=1,
            help='the filename of the image')
    p.add_argument('--verbose', '-v', action='store_true',
            help='print debug messages')
    p.add_argument('--margin', type=int, default=420,
            help='margin to ignore - e.g. where punchholes are located (default: 420 pixels)')
    p.add_argument('--erode', type=int, default=5,
            help='erode threshold - odd number of pixels (default: 5)')
    args = p.parse_args()
    return args

if __name__ == '__main__':
    global args
    args = parse_args()
    setup_logging(args.verbose)
    sys.exit(not is_empty(args.filename[0]))
