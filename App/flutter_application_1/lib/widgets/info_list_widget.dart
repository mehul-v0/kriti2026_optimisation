import 'package:flutter/material.dart';
import 'package:flutter_application_1/theme/theme.dart';

class InfoListWidget extends StatefulWidget {
  final Map<String, dynamic> data;

  const InfoListWidget({super.key, required this.data});

  @override
  State<InfoListWidget> createState() => _InfoListWidgetState();
}

class _InfoListWidgetState extends State<InfoListWidget> {
  int _selectedIndex = 0; // 0 = Employees, 1 = Vehicles

  @override
  Widget build(BuildContext context) {
    // 1. Filter out Empty/Null IDs caused by blank Excel rows
    final rawEmployees = widget.data['employees'] as List? ?? [];
    final employees = rawEmployees.where((e) {
      final id = e['employee_id']?.toString() ?? '';
      return id.isNotEmpty && id.toLowerCase() != 'null';
    }).toList();

    final rawVehicles = widget.data['vehicles'] as List? ?? [];
    final vehicles = rawVehicles.where((v) {
      final id = v['vehicle_id']?.toString() ?? '';
      return id.isNotEmpty && id.toLowerCase() != 'null';
    }).toList();

    final metadata = widget.data['metadata'] as Map<String, dynamic>? ?? {};
    final size = MediaQuery.of(context).size;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        // Metadata Card
        _buildMetadataCard(
          context,
          metadata,
          employees.length,
          vehicles.length,
        ),

        SizedBox(height: size.height * 0.02),

        // Toggle Buttons
        Container(
          height: 50,
          decoration: BoxDecoration(
            color: Colors.grey.shade200,
            borderRadius: BorderRadius.circular(8),
            border: Border.all(color: AppColors.borderColor),
          ),
          child: Row(
            children: [
              Expanded(
                child: _buildTabButton(context, "Employees", 0, Icons.groups),
              ),
              Expanded(
                child: _buildTabButton(context, "Fleet", 1, Icons.local_taxi),
              ),
            ],
          ),
        ),

        SizedBox(height: size.height * 0.02),

        // List Content
        if (_selectedIndex == 0)
          _buildEmployeeList(context, employees)
        else
          _buildVehicleList(context, vehicles),
      ],
    );
  }

  Widget _buildTabButton(
    BuildContext context,
    String title,
    int index,
    IconData icon,
  ) {
    final isSelected = _selectedIndex == index;
    return GestureDetector(
      onTap: () => setState(() => _selectedIndex = index),
      child: Container(
        margin: const EdgeInsets.all(4),
        decoration: BoxDecoration(
          color: isSelected ? context.primary : Colors.transparent,
          borderRadius: BorderRadius.circular(6),
        ),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(
              icon,
              color: isSelected ? Colors.white : Colors.grey.shade700,
              size: 20,
            ),
            const SizedBox(width: 8),
            Text(
              title,
              style: TextStyle(
                color: isSelected ? Colors.white : Colors.grey.shade700,
                fontWeight: FontWeight.bold,
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildEmployeeList(BuildContext context, List employees) {
    if (employees.isEmpty) {
      return Center(
        child: Padding(
          padding: const EdgeInsets.all(20.0),
          child: Text(
            "No valid employees found.",
            style: Theme.of(context).textTheme.bodyMedium,
          ),
        ),
      );
    }

    return ListView.separated(
      physics: const NeverScrollableScrollPhysics(),
      shrinkWrap: true,
      itemCount: employees.length,
      separatorBuilder: (ctx, i) => const SizedBox(height: 8),
      itemBuilder: (ctx, i) {
        return _buildEmployeeItem(context, employees[i]);
      },
    );
  }

  Widget _buildVehicleList(BuildContext context, List vehicles) {
    if (vehicles.isEmpty) {
      return Center(
        child: Padding(
          padding: const EdgeInsets.all(20.0),
          child: Text(
            "No valid vehicles found.",
            style: Theme.of(context).textTheme.bodyMedium,
          ),
        ),
      );
    }

    return ListView.separated(
      physics: const NeverScrollableScrollPhysics(),
      shrinkWrap: true,
      itemCount: vehicles.length,
      separatorBuilder: (ctx, i) => const SizedBox(height: 8),
      itemBuilder: (ctx, i) {
        return _buildVehicleItem(context, vehicles[i]);
      },
    );
  }

  Widget _buildMetadataCard(
    BuildContext context,
    Map meta,
    int empCount,
    int vehCount,
  ) {
    return Card(
      elevation: 0,
      shape: RoundedRectangleBorder(
        side: const BorderSide(color: AppColors.borderColor),
        borderRadius: BorderRadius.circular(12),
      ),
      color: Theme.of(context).cardColor,
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              "City: ${meta['city'] ?? 'Unknown'}",
              style: Theme.of(
                context,
              ).textTheme.headlineSmall?.copyWith(fontSize: 18),
            ),
            const Divider(),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceAround,
              children: [
                _statItem(
                  context,
                  "Employees",
                  empCount.toString(),
                  AppColors.markerEmployee,
                ),
                _statItem(
                  context,
                  "Vehicles",
                  vehCount.toString(),
                  AppColors.markerNormal,
                ),
                _statItem(context, "Priority", "5", AppColors.warning),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _statItem(
    BuildContext context,
    String label,
    String value,
    Color color,
  ) {
    return Column(
      children: [
        Text(
          value,
          style: TextStyle(
            fontSize: 20,
            fontWeight: FontWeight.bold,
            color: color,
          ),
        ),
        Text(
          label,
          style: Theme.of(context).textTheme.bodyMedium?.copyWith(fontSize: 12),
        ),
      ],
    );
  }

  // --- ITEM WIDGETS ---

  Widget _buildEmployeeItem(BuildContext context, Map emp) {
    String rawId = emp['employee_id'].toString();
    String displayId = rawId.length > 1 ? rawId.substring(1) : rawId;

    return Container(
      decoration: BoxDecoration(
        color: Theme.of(context).cardColor,
        border: Border.all(color: AppColors.borderColor),
        borderRadius: BorderRadius.circular(8),
      ),
      child: ListTile(
        leading: CircleAvatar(
          backgroundColor: const Color(0x1A3B82F6),
          child: Text(
            displayId,
            style: const TextStyle(
              color: AppColors.markerEmployee,
              fontWeight: FontWeight.bold,
            ),
          ),
        ),
        title: Text(
          "ID: $rawId",
          style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 14),
        ),
        subtitle: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              "Time: ${emp['earliest_pickup']} - ${emp['latest_drop']}",
              style: const TextStyle(fontSize: 12),
            ),
            Text(
              "Pref: ${emp['vehicle_preference']} | ${emp['sharing_preference']}",
              style: const TextStyle(fontSize: 12),
            ),
          ],
        ),
        trailing: Chip(
          label: Text("P-${emp['priority']}"),
          backgroundColor: AppColors.lightBackground,
          side: BorderSide.none,
          labelStyle: const TextStyle(color: Colors.black87, fontSize: 11),
          padding: EdgeInsets.zero,
        ),
      ),
    );
  }

  Widget _buildVehicleItem(BuildContext context, Map veh) {
    final isPremium = veh['category'].toString().toLowerCase() == 'premium';
    return Container(
      decoration: BoxDecoration(
        color: Theme.of(context).cardColor,
        border: Border.all(color: AppColors.borderColor),
        borderRadius: BorderRadius.circular(8),
      ),
      child: ListTile(
        leading: Icon(
          Icons.directions_car,
          color: isPremium ? AppColors.markerPremium : AppColors.markerNormal,
          size: 28,
        ),
        title: Text(
          veh['vehicle_id'],
          style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 14),
        ),
        subtitle: Text(
          "Cap: ${veh['capacity']} | Cost: ${veh['cost_per_km']}/km",
          style: const TextStyle(fontSize: 12),
        ),
        trailing: Text(
          veh['category'].toString().toUpperCase(),
          style: TextStyle(
            color: isPremium ? AppColors.warning : AppColors.success,
            fontWeight: FontWeight.bold,
            fontSize: 11,
          ),
        ),
      ),
    );
  }
}
