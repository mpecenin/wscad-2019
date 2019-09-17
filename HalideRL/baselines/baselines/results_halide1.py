import os
from glob import glob
from baselines.logger import TensorBoardOutputFormat
import tensorflow as tf

tb_files = glob(os.path.join(os.path.dirname(__file__), '../../result', '**/tb/*tfevents*'), recursive=True)

for file in tb_files:

    dir = os.path.join(os.path.dirname(file), os.path.pardir)
    tb = TensorBoardOutputFormat(os.path.join(dir, 'tb1'))

    for e in tf.train.summary_iterator(file):
        for v in e.summary.value:
            if v.tag == 'EpExecMean':
                v.simple_value = v.simple_value * 1000  # seconds to ms
        tb.writekvs_ev(e)

    tb.close()
