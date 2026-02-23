import 'dart:async';
import 'dart:ui';
import 'package:flutter/material.dart';
import 'package:flutter_application_1/screen/show_input_page.dart';
import 'package:flutter_application_1/services/auth_service.dart';
import 'package:flutter_application_1/services/data_service.dart';
import 'package:flutter_application_1/elements/snackbar.dart';
import 'package:flutter_application_1/elements/sliver_loading.dart';
import 'package:flutter_application_1/theme/theme.dart';
import 'package:flutter_application_1/widgets/add_test_case_dialog.dart';
import 'package:flutter_application_1/widgets/test_case_card.dart';
import 'package:flutter_application_1/widgets/home_drawer.dart';
import 'package:flutter_application_1/widgets/filter_bottom_sheet.dart';

class HomePage extends StatefulWidget {
  // Pass a notifier if you want to control theme from main.dart,
  // otherwise this manages local state for the switch UI.
  final ValueNotifier<ThemeMode>? themeNotifier;

  const HomePage({super.key, this.themeNotifier});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  // Services
  final AuthService _authService = AuthService();
  final DataService _dataService = DataService();
  final ScrollController _scrollController = ScrollController();
  final TextEditingController _searchController = TextEditingController();

  // State
  bool _isLoading = true;
  List<Map<String, dynamic>> _allTestCases = [];
  List<Map<String, dynamic>> _cachedProcessedTestCases = [];
  bool _needsReprocessing = true;

  // Interaction State
  bool _isSelectionMode = false;
  bool _isSearching = false; // State for search bar visibility
  String _searchQuery = "";

  final Set<String> _selectedIds = {};
  final Set<String> _pinnedIds = {};
  SortOption _currentSort = SortOption.dateNewest;
  bool _showScrollToTop = false;

  // Performance optimizations
  Timer? _searchDebounceTimer;
  Timer? _scrollDebounceTimer;
  static const Duration _searchDebounceDelay = Duration(milliseconds: 300);
  static const Duration _scrollDebounceDelay = Duration(milliseconds: 100);

  // Cached grid delegates to avoid recreation on every build
  static const _wideGridDelegate = SliverGridDelegateWithFixedCrossAxisCount(
    crossAxisCount: 2,
    mainAxisSpacing: 12,
    crossAxisSpacing: 12,
    mainAxisExtent: 80,
  );
  static const _narrowGridDelegate = SliverGridDelegateWithFixedCrossAxisCount(
    crossAxisCount: 1,
    mainAxisSpacing: 12,
    crossAxisSpacing: 12,
    mainAxisExtent: 80,
  );

  @override
  void initState() {
    super.initState();
    _loadData();
    _scrollController.addListener(_debouncedScrollListener);

    // Debounced listener for search input
    _searchController.addListener(_onSearchChanged);
  }

  @override
  void dispose() {
    _searchDebounceTimer?.cancel();
    _scrollDebounceTimer?.cancel();
    _scrollController.dispose();
    _searchController.dispose();
    super.dispose();
  }

  void _onSearchChanged() {
    _searchDebounceTimer?.cancel();
    _searchDebounceTimer = Timer(_searchDebounceDelay, () {
      if (mounted) {
        setState(() {
          _searchQuery = _searchController.text.toLowerCase();
          _needsReprocessing = true;
        });
      }
    });
  }

  void _debouncedScrollListener() {
    _scrollDebounceTimer?.cancel();
    _scrollDebounceTimer = Timer(_scrollDebounceDelay, () {
      if (mounted) {
        _scrollListener();
      }
    });
  }

  void _scrollListener() {
    final shouldShowScrollToTop = _scrollController.offset > 200;
    if (shouldShowScrollToTop != _showScrollToTop) {
      setState(() => _showScrollToTop = shouldShowScrollToTop);
    }
  }

  Future<void> _loadData() async {
    try {
      final data = await _dataService.fetchTestCases();
      if (mounted) {
        setState(() {
          _allTestCases = data;
          _needsReprocessing = true;
          _isLoading = false;
        });
      }
    } catch (e) {
      if (mounted) {
        AppSnackbar.show(context, message: e.toString(), isError: true);
        setState(() => _isLoading = false);
      }
    }
  }

  // --- Logic: Sorting & Filtering ---
  List<Map<String, dynamic>> get _processedTestCases {
    if (!_needsReprocessing && _cachedProcessedTestCases.isNotEmpty) {
      return _cachedProcessedTestCases;
    }

    List<Map<String, dynamic>> list;

    // Filter only if there's a search query
    if (_searchQuery.isNotEmpty) {
      list = _allTestCases.where((item) {
        final name = (item['case_name'] ?? '').toLowerCase();
        return name.contains(_searchQuery);
      }).toList();
    } else {
      list = List.from(_allTestCases);
    }

    // 1. Sort based on option
    list.sort((a, b) {
      // First check pinned status
      final aPinned = _pinnedIds.contains(a['id'].toString());
      final bPinned = _pinnedIds.contains(b['id'].toString());
      if (aPinned && !bPinned) return -1;
      if (!aPinned && bPinned) return 1;

      // Then sort by selected option
      switch (_currentSort) {
        case SortOption.nameAsc:
          return (a['case_name'] ?? '').compareTo(b['case_name'] ?? '');
        case SortOption.nameDesc:
          return (b['case_name'] ?? '').compareTo(a['case_name'] ?? '');
        case SortOption.dateOldest:
          return (a['created_at'] ?? '').compareTo(b['created_at'] ?? '');
        case SortOption.dateNewest:
          return (b['created_at'] ?? '').compareTo(a['created_at'] ?? '');
      }
    });

    _cachedProcessedTestCases = list;
    _needsReprocessing = false;
    return list;
  }

  // --- Logic: Selection ---
  void _toggleSelection(String id) {
    setState(() {
      if (_selectedIds.contains(id)) {
        _selectedIds.remove(id);
        if (_selectedIds.isEmpty) _isSelectionMode = false;
      } else {
        _selectedIds.add(id);
        _isSelectionMode = true;
      }
    });
  }

  void _selectAll() {
    setState(() {
      if (_selectedIds.length == _allTestCases.length) {
        _selectedIds.clear();
        _isSelectionMode = false;
      } else {
        _selectedIds.addAll(_allTestCases.map((e) => e['id'].toString()));
        _isSelectionMode = true;
      }
    });
  }

  void _togglePin(String id) {
    setState(() {
      if (_pinnedIds.contains(id)) {
        _pinnedIds.remove(id);
      } else {
        _pinnedIds.add(id);
      }
      _needsReprocessing = true;
    });
  }

  // --- Logic: Deletion ---
  Future<void> _deleteItems(List<String> ids) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text("Confirm Delete"),
        content: Text("Are you sure you want to delete ${ids.length} item(s)?"),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text("Cancel"),
          ),
          TextButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text(
              "Delete",
              style: TextStyle(color: AppColors.error),
            ),
          ),
        ],
      ),
    );

    if (confirmed == true) {
      setState(() => _isLoading = true);
      // Batch delete in parallel instead of sequentially
      await Future.wait(ids.map((id) => _dataService.deleteTestCase(id)));
      _selectedIds.clear();
      _isSelectionMode = false;
      await _loadData();
    }
  }

  void _showSortMenu() {
    FilterBottomSheet.show(
      context,
      currentSort: _currentSort,
      onSortChanged: (newSort) {
        setState(() {
          _currentSort = newSort;
          _needsReprocessing = true;
        });
      },
    );
  }

  void _cancelSelectionMode() {
    setState(() {
      _isSelectionMode = false;
      _selectedIds.clear();
    });
  }

  // --- Logic: Rename ---
  Future<void> _renameItem(String id, String currentName) async {
    final renameController = TextEditingController(text: currentName);
    final newName = await showDialog<String>(
      context: context,
      builder: (ctx) {
        return AlertDialog(
          title: const Text("Rename Test Case"),
          content: TextField(
            controller: renameController,
            autofocus: true,
            decoration: InputDecoration(
              labelText: "Case Name",
              hintText: "Enter new name",
              border: OutlineInputBorder(
                borderRadius: BorderRadius.circular(12),
              ),
              focusedBorder: OutlineInputBorder(
                borderRadius: BorderRadius.circular(12),
                borderSide: const BorderSide(
                  color: AppColors.primaryBrand,
                  width: 2,
                ),
              ),
            ),
            onSubmitted: (val) => Navigator.pop(ctx, val.trim()),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(ctx),
              child: const Text("Cancel"),
            ),
            TextButton(
              onPressed: () => Navigator.pop(ctx, renameController.text.trim()),
              child: const Text(
                "Rename",
                style: TextStyle(color: AppColors.primaryBrand),
              ),
            ),
          ],
        );
      },
    );

    if (newName != null && newName.isNotEmpty && newName != currentName) {
      try {
        await _dataService.renameTestCase(id, newName);
        if (mounted) {
          AppSnackbar.show(context, message: "Renamed to \"$newName\"");
          await _loadData();
        }
      } catch (e) {
        if (mounted) {
          AppSnackbar.show(
            context,
            message: "Rename failed: $e",
            isError: true,
          );
        }
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    // Use efficient MediaQuery lookups that only subscribe to specific changes
    final size = MediaQuery.sizeOf(context);
    final bottomPadding = MediaQuery.paddingOf(context).bottom;
    final isWide = size.width > 700;
    final displayList = _processedTestCases;

    return PopScope(
      canPop: !_isSelectionMode,
      onPopInvokedWithResult: (bool didPop, dynamic result) {
        if (!didPop && _isSelectionMode) {
          _cancelSelectionMode();
        }
      },
      child: Scaffold(
        drawer: _buildDrawer(),
        body: SafeArea(
          child: Scrollbar(
            controller: _scrollController,
            thumbVisibility: true,
            thickness: 6,
            radius: const Radius.circular(3),
            child: CustomScrollView(
              controller: _scrollController,
              slivers: [
                _buildSliverAppBar(),

                if (_isLoading)
                  const SliverLoading()
                else if (displayList.isEmpty)
                  SliverFillRemaining(
                    child: _searchQuery.isNotEmpty
                        ? _buildNoSearchResults()
                        : _buildEmptyState(),
                  )
                else
                  SliverPadding(
                    padding: EdgeInsets.fromLTRB(
                      isWide ? size.width * 0.1 : 16,
                      16,
                      isWide ? size.width * 0.1 : 16,
                      100 + bottomPadding,
                    ),
                    sliver: SliverGrid(
                      gridDelegate: isWide
                          ? _wideGridDelegate
                          : _narrowGridDelegate,
                      delegate: SliverChildBuilderDelegate(
                        (context, index) {
                          final item = displayList[index];
                          final idStr = item['id'].toString();
                          return RepaintBoundary(
                            child: TestCaseCard(
                              key: ValueKey(idStr),
                              data: item,
                              isSelected: _selectedIds.contains(idStr),
                              isSelectionMode: _isSelectionMode,
                              isPinned: _pinnedIds.contains(idStr),
                              onTap: () {
                                if (_isSelectionMode) {
                                  _toggleSelection(idStr);
                                } else {
                                  Navigator.push(
                                    context,
                                    MaterialPageRoute(
                                      builder: (context) => ShowInputPage(
                                        testCaseId: item['id'],
                                        testCaseName: item['case_name'],
                                        data: item['input_data'],
                                      ),
                                    ),
                                  );
                                }
                              },
                              onLongPress: () => _toggleSelection(idStr),
                              onPinToggle: () => _togglePin(idStr),
                              onDelete: () => _deleteItems([idStr]),
                              onRename: () =>
                                  _renameItem(idStr, item['case_name'] ?? ''),
                            ),
                          );
                        },
                        childCount: displayList.length,
                        addAutomaticKeepAlives: false,
                      ),
                    ),
                  ),
              ],
            ),
          ),
        ),
        floatingActionButtonLocation: FloatingActionButtonLocation.endFloat,
        floatingActionButton: Padding(
          padding: EdgeInsets.only(bottom: bottomPadding + 8),
          child: _buildFABs(),
        ),
      ),
    );
  }

  Widget _buildSliverAppBar() {
    return SliverAppBar(
      expandedHeight: _isSearching ? 56.0 : 140.0,
      floating: false,
      pinned: true,
      backgroundColor: _isSelectionMode
          ? AppColors.darkBrand
          : AppColors.primaryBrand,
      iconTheme: const IconThemeData(color: Colors.white),
      actions: _isSelectionMode
          ? _buildSelectionActions()
          : _buildStandardActions(),
      title: _isSearching ? _buildSearchBar() : null,
      flexibleSpace: _isSearching
          ? null
          : FlexibleSpaceBar(
              titlePadding: const EdgeInsetsDirectional.only(
                start: 16.0,
                bottom: 16.0,
                top: 56.0,
              ),
              expandedTitleScale: 1.0,
              title: Text(
                _isSelectionMode
                    ? "${_selectedIds.length} Selected"
                    : "Optimization Dashboard",
                style: const TextStyle(
                  color: Colors.white,
                  fontWeight: FontWeight.bold,
                  fontSize: 22,
                ),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
              ),
              background: Container(
                decoration: BoxDecoration(
                  gradient: LinearGradient(
                    begin: Alignment.topLeft,
                    end: Alignment.bottomRight,
                    colors: [
                      _isSelectionMode
                          ? AppColors.darkBrand
                          : AppColors.primaryBrand,
                      _isSelectionMode ? Colors.black87 : AppColors.darkBrand,
                    ],
                  ),
                ),
              ),
            ),
    );
  }

  // --- Search Bar Widget ---
  Widget _buildSearchBar() {
    final isDark = Theme.of(context).brightness == Brightness.dark;
    final textColor = isDark ? Colors.white : Colors.black87;
    final hintColor = isDark ? const Color(0x99FFFFFF) : Colors.black54;
    final iconColor = isDark ? const Color(0xB3FFFFFF) : Colors.black54;
    final bgColor = isDark ? const Color(0x26FFFFFF) : const Color(0x14000000);

    return Container(
      height: 40,
      decoration: BoxDecoration(
        color: bgColor,
        borderRadius: BorderRadius.circular(10),
      ),
      child: TextField(
        controller: _searchController,
        autofocus: true,
        style: TextStyle(color: textColor, fontSize: 16),
        cursorColor: textColor,
        decoration: InputDecoration(
          hintText: "Search test cases...",
          hintStyle: TextStyle(color: hintColor, fontSize: 15),
          border: InputBorder.none,
          contentPadding: const EdgeInsets.symmetric(
            horizontal: 14,
            vertical: 10,
          ),
          isDense: true,
          prefixIcon: Icon(Icons.search, color: iconColor, size: 20),
          prefixIconConstraints: const BoxConstraints(minWidth: 40),
        ),
      ),
    );
  }

  List<Widget> _buildStandardActions() {
    if (_isSearching) {
      return [
        IconButton(
          icon: const Icon(Icons.close),
          onPressed: () {
            _searchDebounceTimer?.cancel();
            setState(() {
              _isSearching = false;
              _searchQuery = "";
              _searchController.clear();
              _needsReprocessing = true;
            });
          },
        ),
      ];
    }

    return [
      IconButton(
        icon: const Icon(Icons.search),
        onPressed: () {
          setState(() {
            _isSearching = true;
          });
        },
      ),
      IconButton(icon: const Icon(Icons.filter_list), onPressed: _showSortMenu),
      const SizedBox(width: 8),
    ];
  }

  List<Widget> _buildSelectionActions() {
    return [
      TextButton.icon(
        onPressed: _selectAll,
        icon: const Icon(Icons.select_all, color: Colors.white),
        label: const Text("All", style: TextStyle(color: Colors.white)),
      ),
      IconButton(
        icon: const Icon(Icons.delete),
        onPressed: () => _deleteItems(_selectedIds.toList()),
      ),
    ];
  }

  Widget _buildFABs() {
    if (_isSelectionMode) return const SizedBox.shrink();

    return Column(
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.end,
      children: [
        if (_showScrollToTop)
          Padding(
            padding: const EdgeInsets.only(bottom: 16),
            child: GestureDetector(
              onTap: () {
                _scrollController.animateTo(
                  0,
                  duration: const Duration(milliseconds: 500),
                  curve: Curves.easeOut,
                );
              },
              child: ClipRRect(
                borderRadius: BorderRadius.circular(16),
                child: BackdropFilter(
                  filter: ImageFilter.blur(sigmaX: 14, sigmaY: 14),
                  child: Builder(
                    builder: (ctx) {
                      final isDark =
                          Theme.of(ctx).brightness == Brightness.dark;
                      return Container(
                        width: 40,
                        height: 40,
                        decoration: BoxDecoration(
                          color: AppColors.primaryBrand,
                          borderRadius: BorderRadius.circular(16),
                          border: isDark
                              ? Border.all(
                                  color: Colors.white.withOpacity(0.6),
                                  width: 1.2,
                                )
                              : null,
                        ),
                        child: const Icon(
                          Icons.keyboard_arrow_up_rounded,
                          color: Colors.white,
                          size: 22,
                        ),
                      );
                    },
                  ),
                ),
              ),
            ),
          ),
        GestureDetector(
          onTap: () {
            showDialog(
              context: context,
              barrierDismissible: false,
              builder: (ctx) => AddTestCaseDialog(
                onSuccess: () {
                  Navigator.pop(ctx);
                  AppSnackbar.show(
                    context,
                    message: "Test Case Uploaded Successfully",
                  );
                  _loadData();
                },
              ),
            );
          },
          child: ClipRRect(
            borderRadius: BorderRadius.circular(18),
            child: BackdropFilter(
              filter: ImageFilter.blur(sigmaX: 14, sigmaY: 14),
              child: Builder(
                builder: (ctx) {
                  final isDark = Theme.of(ctx).brightness == Brightness.dark;
                  return Container(
                    width: 56,
                    height: 56,
                    decoration: BoxDecoration(
                      color: AppColors.primaryBrand,
                      borderRadius: BorderRadius.circular(18),
                      border: isDark
                          ? Border.all(
                              color: Colors.white.withOpacity(0.6),
                              width: 1.2,
                            )
                          : null,
                    ),
                    child: const Icon(Icons.add, color: Colors.white, size: 28),
                  );
                },
              ),
            ),
          ),
        ),
      ],
    );
  }

  Widget _buildEmptyState() {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Icon(Icons.folder_off_outlined, size: 80, color: Colors.grey[300]),
          const SizedBox(height: 16),
          Text(
            "No test cases yet",
            style: Theme.of(
              context,
            ).textTheme.titleLarge?.copyWith(color: Colors.grey),
          ),
          const SizedBox(height: 8),
          const Text("Click the + button to upload your first case."),
        ],
      ),
    );
  }

  Widget _buildNoSearchResults() {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Icon(Icons.search_off, size: 80, color: Colors.grey[300]),
          const SizedBox(height: 16),
          Text(
            "No results found",
            style: Theme.of(
              context,
            ).textTheme.titleLarge?.copyWith(color: Colors.grey),
          ),
        ],
      ),
    );
  }

  Widget _buildDrawer() {
    final userEmail = _authService.currentUser?.email ?? "student@iitg.ac.in";

    return HomeDrawer(
      userEmail: userEmail,
      themeNotifier: widget.themeNotifier,
      onLogout: () async {
        await _authService.signOut();
      },
    );
  }
}
