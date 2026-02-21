import 'package:flutter/material.dart';
import 'package:flutter_application_1/theme/theme.dart';

/// Drawer for the HomePage with user profile, theme toggle, and logout
class HomeDrawer extends StatelessWidget {
  final String userEmail;
  final ValueNotifier<ThemeMode>? themeNotifier;
  final VoidCallback onLogout;

  const HomeDrawer({
    Key? key,
    required this.userEmail,
    required this.themeNotifier,
    required this.onLogout,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    // Read theme directly from context so drawer always has correct state
    final isDarkMode = Theme.of(context).brightness == Brightness.dark;

    return Drawer(
      child: Column(
        children: [
          // User Profile Header
          UserAccountsDrawerHeader(
            decoration: const BoxDecoration(
              gradient: LinearGradient(
                begin: Alignment.topLeft,
                end: Alignment.bottomRight,
                colors: [AppColors.primaryBrand, AppColors.darkBrand],
              ),
            ),
            accountName: null,
            accountEmail: Text(
              userEmail,
              style: const TextStyle(
                fontSize: 16,
                fontWeight: FontWeight.w600,
                color: Colors.white,
              ),
            ),
            currentAccountPicture: Container(
              decoration: const BoxDecoration(
                shape: BoxShape.circle,
                color: Colors.white,
                boxShadow: [
                  BoxShadow(
                    color: Color(0x33000000),
                    blurRadius: 8,
                    offset: Offset(0, 2),
                  ),
                ],
              ),
              child: const CircleAvatar(
                backgroundColor: Colors.white,
                child: Icon(
                  Icons.person,
                  color: AppColors.primaryBrand,
                  size: 40,
                ),
              ),
            ),
          ),

          // Theme Toggle
          ListTile(
            leading: Icon(
              isDarkMode ? Icons.dark_mode : Icons.light_mode,
              color: AppColors.primaryBrand,
            ),
            title: Text(isDarkMode ? "Dark Mode" : "Light Mode"),
            trailing: Switch(
              value: isDarkMode,
              onChanged: (val) {
                if (themeNotifier != null) {
                  themeNotifier!.value = isDarkMode
                      ? ThemeMode.light
                      : ThemeMode.dark;
                }
              },
              activeColor: AppColors.primaryBrand,
            ),
          ),

          const Divider(height: 1),

          // About/Info Section (Optional)
          ListTile(
            leading: const Icon(Icons.info_outline),
            title: const Text("About"),
            onTap: () {
              Navigator.pop(context);
              _showAboutDialog(context);
            },
          ),

          const Spacer(),
          const Divider(),

          // Logout
          ListTile(
            leading: const Icon(Icons.logout, color: AppColors.error),
            title: const Text(
              "Logout",
              style: TextStyle(
                color: AppColors.error,
                fontWeight: FontWeight.w600,
              ),
            ),
            onTap: () {
              Navigator.pop(context);
              onLogout();
            },
          ),

          // Safe Area Bottom Padding
          SizedBox(height: MediaQuery.of(context).padding.bottom + 10),
        ],
      ),
    );
  }

  void _showAboutDialog(BuildContext context) {
    showAboutDialog(
      context: context,
      applicationName: "Optimization Dashboard",
      applicationVersion: "1.0.0",
      applicationIcon: Container(
        padding: const EdgeInsets.all(8),
        decoration: BoxDecoration(
          color: const Color(0x1A00C569),
          borderRadius: BorderRadius.circular(12),
        ),
        child: const Icon(
          Icons.dashboard,
          color: AppColors.primaryBrand,
          size: 40,
        ),
      ),
      children: [
        const Text("A powerful tool for vehicle routing optimization."),
        const SizedBox(height: 8),
        const Text("Developed for efficient test case management."),
      ],
    );
  }
}
