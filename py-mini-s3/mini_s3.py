#! /usr/bin/python2.4

"""Minimalistic client for Amazon S3 (AWS) in Python.

This module supports the S3 GET and PUT operations over HTTP (not HTTPS).

This module is written for educational purposes for those wanting to implement
their own client.

See http://docs.amazonwebservices.com/AmazonS3/latest/index.html?RESTAuthentication.html
and for the documentation of the protocol.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
"""

__author__ = 'pts@fazekas.hu (Peter Szabo)'

import base64
import hmac
import os
import sha
import time
import httplib
import urllib

AWS_S3_HOST = os.getenv('AWS_S3_HOST', 's3.amazonaws.com')
AWS_SECRET_ACCESS_KEY = os.getenv('AWS_SECRET_ACCESS_KEY', '')
assert AWS_SECRET_ACCESS_KEY, 'please export AWS_SECRET_ACCESS_KEY='
AWS_ACCESS_KEY_ID = os.getenv('AWS_ACCESS_KEY_ID', '')
assert AWS_ACCESS_KEY_ID, 'please export AWS_ACCESS_KEY_ID='

WDAY = ('Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun')
MON = ('', 'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep',
       'Oct', 'Nov', 'Dec')

def CreateHttp(host, method, content_type, old_http_class):
  """Helper function for implementing the HTTP put method in urllib."""
  http = old_http_class(host)
  old_putrequest = http.putrequest
  old_putheader = http.putheader
  force_method = method

  def PutRequest(method, selector):
    old_putrequest(force_method, selector)
  
  def PutHeader(key, value):
    if key.lower() == 'content-type':
      value = content_type
    old_putheader(key, value)

  http.putrequest = PutRequest
  http.putheader = PutHeader
  
  return http


class MyUrlOpener(urllib.URLopener):
  def http_error_default(self, url, fp, errcode, errmsg, headers):
    try:
      data = fp.read()
    finally:
      fp.close()
    # URLopener doesn't append `data' here.
    raise IOError('http error', errcode, errmsg, headers, data)


def Sign(method, bucket, key, date, content_md5, content_type, amz_headers):
  assert '/' not in bucket
  assert not key.startswith('/')
  if amz_headers:
    raise NotImplementedError
  else:
    canonicalized_amz_headers = ''
  canonicalized_resource = '/%s/%s' % (bucket, key)
  string_to_sign = '%s\n%s\n%s\n%s\n%s%s' % (  # UTF-8
      method, content_md5, content_type, date,
      canonicalized_amz_headers, canonicalized_resource)
  return base64.encodestring(hmac.new(
      AWS_SECRET_ACCESS_KEY, string_to_sign, sha).digest()).rstrip()

def Open(method, bucket, key, data=None, content_type='', now=None,
         amz_headers=None):
  assert method in ('GET', 'PUT')
  assert method != 'PUT' or isinstance(data, str)
  assert method != 'GET' or data is None
  if now is None:
    # This must be close to accurate, otherwise we get an error in GET.
    # For PUT, any date can be specified (??).
    now = time.gmtime()
  if isinstance(now, float):
    now = time.gmtime(now)
  if not isinstance(now, time.struct_time):
    raise TypeError, type(now)
  date = '%s, %2d %s %d %2d:%02d:%02d GMT' % (
      WDAY[now[6]], now[2], MON[now[1]], now[0], now[3], now[4], now[5])
  # We mustn't set this if we don't set the content-md5 HTTP header.    
  #content_md5 = base64.encodestring(md5.new(data).digest()).rstrip()
  signature = Sign(method=method, bucket=bucket, key=key,
                   date=date, content_md5='',
                   content_type=content_type, amz_headers=amz_headers)
  opener = MyUrlOpener()
  opener.addheader('Date', date)
  # If Authorization is wrong, we'll get forbidden.
  opener.addheader(
      'Authorization', 'AWS %s:%s' % (AWS_ACCESS_KEY_ID, signature))
  url = 'http://%s/%s/%s' % (AWS_S3_HOST, bucket, key)
  if method == 'PUT':
    # TODO(pts): Make the PUT method thread-safe.
    old_http_class = httplib.HTTP
    try:
      httplib.HTTP = lambda host: CreateHttp(
          host, method, content_type, old_http_class)
      return opener.open(url, data)
    finally:
      httplib.HTTP = old_http_class
  else:
    return opener.open(url)

def Get(bucket, key):
  """Fetch the value in an S3 bucket.

  Args:
    bucket: String containing the name of the S3 bucket.
    key: String containing the key within the S3 bucket.
  Returns:
    (headers, value), where value is the value for the key in the specified
      S3 bucket, and headers is a httplib.HTTPMessage instance containing
  Raises:
    IOError: For various download problems.
      HTTP 403 'Forbidden' if AWS_SECRET_ACCESS_KEY is wrong.
      HTTP 404 'NotFound' if the specified bucket:key pair doesn't exist.
    *: Whatever urllib raises.
  """
  f = Open(method='GET', bucket=bucket, key=key)
  try:
    return f.info(), f.read()
  finally:
    f.close()

def Put(bucket, key, value, content_type=''):
  """Change a key in an S3 bucket.

  Args:
    bucket: String containing the name of the S3 bucket.
    key: String containing the key within the S3 bucket.
    value: String containing the value to set bucket:key to.
    content_type: The HTTP content type. Can be empty (will be changed to
      binary/octet-stream).
  Raises:
    IOError: For various download problems.
      We may get a HTTP 400 'Bad request' raised for PUT (not GET) if the
      Authorization is wrong or the Date is missing.
    *: Whatever urllib raises.
  """
  f = Open(method='PUT', bucket=bucket, key=key, data=value,
           content_type=content_type)
  try:
    f.read()
  finally:
    f.close()


if __name__ == '__main__':
  import sys
  assert len(sys.argv) == 3, 'Usage: %s <bucket> <key>' % sys.argv[0]
  bucket = sys.argv[1]
  key = sys.argv[2]
  try:
    headers, data = Get(bucket=bucket, key=key)
  except IOError, e:
    if e.args[0] == 'http error' and e.args[1] == 404:
      print e.args
      print 'Not found.'
      data = None
      headers = e.args[3]
  print repr(headers)
  print repr(headers['content-type'])
  print repr(data) 
  print Put(bucket=bucket, key=key,
            value=('%s is a much longer value\n' % time.time()) * 20)
