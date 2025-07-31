import requests

data = {
    "co2": 700,
    "vis": 700,
    "veh_c1": 3,
    "veh_c2": 1
}

response = requests.post("http://localhost:5000/decidir", json=data)
print(response.json())
