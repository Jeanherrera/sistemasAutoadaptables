import requests

# action = 0 → prioridad carril 1
# action = 1 → prioridad carril 2
# action = 2 → modo sin vehículos
# action = 3 → cruce peatonal

data = {
    "veh_c1": 2,
    "veh_c2": 3,
    "co2": 1000,                # Mayor a 700 → co2High = 1
    "vlS1": 700,               # Menor a 600 → vlS1 = 1
    "vlS2": 700,               # Menor a 600 → vlS2 = 1
    "sensor_other_city": 0,    # 1 o 0
    "currentMode": 1,          # 0: Día, 1: Noche
    "peatonalRequested": 0     # 1 si botón fue oprimido
}

response = requests.post("http://localhost:5000/decidir", json=data)
print(response.json())

#response = requests.post("http://localhost:5000/decidir", json=data)
#print(response.json())
