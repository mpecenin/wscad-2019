import os
from glob import glob
from baselines.bench.monitor import load_results
from baselines.logger import TensorBoardOutputFormat
from collections import deque
import numpy as np

monitor_files = glob(os.path.join(os.path.dirname(__file__), '../../result', '**/monitor.csv'), recursive=True)

for monitor in monitor_files:

    dir = os.path.dirname(monitor)
    csv = load_results(dir)
    tb = TensorBoardOutputFormat(os.path.join(dir, 'tb2'))

    length = 100
    kv = {}

    for i in range(length, csv.r.size):
        t = csv.t.values[i]
        r = csv.r.values[i-length:i]
        l = csv.l.values[i-length:i]
        e = csv.best_exec.values[i-length:i] * 1000  # seconds to ms
        kv['EpExecMean'] = np.mean(e)
        kv['EpRewMean'] = np.mean(r)
        kv['EpLenMean'] = np.mean(l)
        tb.writekvs_wt(kv, t)

    tb.close()
