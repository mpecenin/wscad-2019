import os
from glob import glob
import pandas

progress_files = sorted(glob(os.path.join(os.path.dirname(__file__), '../../result', '**/progress.csv'), recursive=True))

csv_list = []
csv_columns = []
e = 'EpExecMean'
r = 'EpRewMean'

for i, file in enumerate(progress_files):
    csv = pandas.read_csv(file)
    for j in range(0, csv.EpExecMean.size):
        csv.EpExecMean.values[j] *= 1000  # seconds to ms
    x = str(i+1).zfill(2)
    csv.rename(columns={e: e+x, r: r+x}, inplace=True)
    csv_columns.extend([e+x, r+x])
    csv_list.append(csv)

out = pandas.concat(csv_list, axis=1, sort=False)
out.to_csv('tb3_progress.csv', index_label='index', columns=csv_columns)
