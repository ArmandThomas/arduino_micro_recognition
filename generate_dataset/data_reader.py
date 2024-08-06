import serial
import time
import pandas as pd
import sys

# 0 -> glass, 1 -> background, 2 -> siren
# Configuration des paramètres
target = 0
num_samples = 8000
serial_port = 'COM3'  # Assurez-vous que ce port est correct
baud_rate = 9600
timeout = 10

# Initialiser la connexion série
try:
    arduino = serial.Serial(serial_port, baud_rate, timeout=timeout)
    print(f"Connexion série établie sur {serial_port} avec un débit de {baud_rate}.")
except Exception as e:
    print(f"Erreur lors de l'ouverture du port série: {e}")
    sys.exit()

nbrRecords = 0

def clean_data(data_str):
    """ Nettoyer les données pour enlever les valeurs non numériques. """
    data_str = data_str.strip()  # Enlever les espaces blancs au début et à la fin
    data_str = data_str.rstrip(",")  # Enlever les virgules de fin
    data_values = data_str.split(",")  # Diviser les valeurs par la virgule
    cleaned_values = []
    for value in data_values:
        try:
            cleaned_values.append(int(value.strip()))  # Enlever les espaces et convertir en entier
        except ValueError:
            continue  # Ignorer les valeurs non numériques
    return cleaned_values

try:
    while True:
        data = arduino.readline()
        cleaned_data = clean_data(data.decode('utf-8', errors='replace'))
        if (len(cleaned_data) == num_samples):
            df = pd.DataFrame([cleaned_data], columns=[f'sample_{i+1}' for i in range(num_samples)])
            df['target'] = target
            filename = f'output/sample_data_{nbrRecords}_{target}.csv'

            # Enregistrer dans le fichier CSV
            df.to_csv(filename, index=False)

            print(f"Données enregistrées dans {filename}.")

            nbrRecords += 1

        if nbrRecords == 100:
            print("Fin des enregistrements.")
            sys.exit()
except KeyboardInterrupt:
    print("Interruption par l'utilisateur.")

finally:
    arduino.close()
