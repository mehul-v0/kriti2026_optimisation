import json
d=json.load(open('test_case1_cpp_output.json'))
print('C++ Trip Details:')
for v in d['details']:
    if v['employees']:
        print(f"\nVehicle {v['vehicle']}:")
        for trip in v['trip_routes']:
            print(f"  Trip {trip['trip_number']}: {trip['employees']}")
