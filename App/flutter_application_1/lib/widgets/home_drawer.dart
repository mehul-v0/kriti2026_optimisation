import 'package:flutter/material.dart';
import 'package:flutter_application_1/theme/theme.dart';

class HomeDrawer extends StatelessWidget {
  final String userEmail;
  final ValueNotifier<ThemeMode>? themeNotifier;
  final ValueNotifier<int>? themeIndexNotifier;
  final VoidCallback onLogout;

  const HomeDrawer({
    Key? key,
    required this.userEmail,
    required this.onLogout,
    this.themeNotifier,
    this.themeIndexNotifier,
  }) : super(key: key);

  static const _themeLabels = ['Petronas', 'Orange', 'Yellow'];
  static const _themeColors = [
    Color(0xFF00D2BE),
    Color(0xFFFF8000),
    Color(0xFFF5B800),
  ];

  // Static fallback so ValueListenableBuilder always has a valid listenable
  static final _fallbackIndex = ValueNotifier(0);

  @override
  Widget build(BuildContext context) {
    final isDark = Theme.of(context).brightness == Brightness.dark;
    final primary = Theme.of(context).colorScheme.primary;
    final container = Theme.of(context).colorScheme.primaryContainer;

    return ValueListenableBuilder<int>(
      valueListenable: themeIndexNotifier ?? _fallbackIndex,
      builder: (context, themeIndex, _) {
        return Drawer(
          child: Column(
            children: [
              // ── Profile header ──
              UserAccountsDrawerHeader(
                decoration: BoxDecoration(
                  gradient: LinearGradient(
                    begin: Alignment.topLeft,
                    end: Alignment.bottomRight,
                    colors: [primary, container],
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
                  child: CircleAvatar(
                    backgroundColor: Colors.white,
                    child: Icon(Icons.person, color: primary, size: 40),
                  ),
                ),
              ),

              // ── Colour theme picker ──
              Padding(
                padding: const EdgeInsets.fromLTRB(16, 14, 16, 6),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      'COLOUR THEME',
                      style: TextStyle(
                        color: Theme.of(
                          context,
                        ).colorScheme.onSurface.withOpacity(0.45),
                        fontSize: 10,
                        fontWeight: FontWeight.w700,
                        letterSpacing: 1.5,
                      ),
                    ),
                    const SizedBox(height: 10),
                    Row(
                      children: List.generate(_themeLabels.length, (i) {
                        final color = _themeColors[i];
                        final selected = i == themeIndex;
                        return Expanded(
                          child: Padding(
                            padding: EdgeInsets.only(
                              right: i < _themeLabels.length - 1 ? 8 : 0,
                            ),
                            child: GestureDetector(
                              onTap: () => themeIndexNotifier?.value = i,
                              child: AnimatedContainer(
                                duration: const Duration(milliseconds: 200),
                                padding: const EdgeInsets.symmetric(
                                  vertical: 10,
                                ),
                                decoration: BoxDecoration(
                                  color: selected
                                      ? color.withOpacity(0.15)
                                      : Colors.transparent,
                                  borderRadius: BorderRadius.circular(10),
                                  border: Border.all(
                                    color: selected
                                        ? color
                                        : Theme.of(context).dividerColor,
                                    width: selected ? 1.5 : 1.0,
                                  ),
                                ),
                                child: Column(
                                  children: [
                                    Container(
                                      width: 22,
                                      height: 22,
                                      decoration: BoxDecoration(
                                        color: color,
                                        shape: BoxShape.circle,
                                      ),
                                      child: selected
                                          ? const Icon(
                                              Icons.check,
                                              color: Colors.black,
                                              size: 14,
                                            )
                                          : null,
                                    ),
                                    const SizedBox(height: 5),
                                    Text(
                                      _themeLabels[i],
                                      style: TextStyle(
                                        color: selected
                                            ? color
                                            : Theme.of(context)
                                                  .colorScheme
                                                  .onSurface
                                                  .withOpacity(0.55),
                                        fontSize: 10,
                                        fontWeight: selected
                                            ? FontWeight.w700
                                            : FontWeight.w400,
                                      ),
                                    ),
                                  ],
                                ),
                              ),
                            ),
                          ),
                        );
                      }),
                    ),
                  ],
                ),
              ),

              const Divider(height: 20),

              // ── Light / Dark toggle ──
              ListTile(
                leading: Icon(
                  isDark ? Icons.dark_mode : Icons.light_mode,
                  color: primary,
                ),
                title: Text(isDark ? 'Dark Mode' : 'Light Mode'),
                trailing: Switch(
                  value: isDark,
                  onChanged: (_) {
                    themeNotifier?.value = isDark
                        ? ThemeMode.light
                        : ThemeMode.dark;
                  },
                  activeColor: primary,
                ),
              ),

              const Spacer(),
              const Divider(),

              // ── Logout ──
              ListTile(
                leading: const Icon(Icons.logout, color: AppColors.error),
                title: const Text(
                  'Logout',
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

              SizedBox(height: MediaQuery.of(context).padding.bottom + 10),
            ],
          ),
        );
      },
    );
  }
}
