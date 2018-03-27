# -*- coding: utf-8 -*-
# filename: nlms_numpy.py

import numpy as np
import matplotlib.pyplot as pl

# 用Numpy实现的NLMS算法
# x为参照信号，d为目标信号，h为自适应滤波器的初值
# step_size为更新系数
def nlms(x, d, h, step_size=0.5):
   i = len(h)
   size = len(x)
   # 计算输入到h中的参照信号的乘方he
   power = np.sum( x[i:i-len(h):-1] * x[i:i-len(h):-1] )
   u = np.zeros(size, dtype=np.float64)

   while True:
       x_input = x[i:i-len(h):-1]
       u[i] = np.dot(x_input , h)
       e = d[i] - u[i]
       h += step_size * e / power * x_input

       power -= x_input[-1] * x_input[-1] # 减去最早的取样
       i+=1
       if i >= size: return u
       power += x[i] * x[i] # 增加最新的取样


def make_path(delay, length):
   path_length = length - delay
   h = np.zeros(length, np.float64)
   h[delay:] = np.random.standard_normal(path_length) * np.exp( np.linspace(0, -4, path_length) )
   h /= np.sqrt(np.sum(h*h))
   return h

def plot_converge(y, u, label=""):
    size = len(u)
    avg_number = 200
    e = np.power(y[:size] - u, 2)
    tmp = e[:int(size/avg_number)*avg_number]
    tmp.shape = -1, avg_number
    avg = np.average( tmp, axis=1 )
    pl.plot(np.linspace(0, size, len(avg)), 10*np.log10(avg), linewidth=2.0, label=label)

def diff_db(h0, h):
   return 10*np.log10(np.sum((h0-h)*(h0-h)) / np.sum(h0*h0))


