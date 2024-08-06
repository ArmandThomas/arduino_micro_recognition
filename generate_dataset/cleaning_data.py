import pandas as pd


import os
import glob

# Chemin vers le répertoire contenant les fichiers CSV
input_dir = "output"
output_file = "result/combined_data.csv"

directory = 'output'

files = glob.glob(os.path.join(input_dir, '*.csv'))

dataframes = []

def process_file(file_path, session_value):
    # Lire le fichier CSV
    df = pd.read_csv(file_path)
    dataframes.append(df)

for index, file in enumerate(files):
    print(file)
    process_file(file, index)

combined_df = pd.concat(dataframes, ignore_index=True)


combined_df.to_csv(output_file, index=False)