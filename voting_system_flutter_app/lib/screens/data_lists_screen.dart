import 'package:flutter/material.dart';
import 'package:sqflite/sqflite.dart';
import '../models/student_data.dart';

class DataListScreen extends StatefulWidget {
  final Database database;
  final VoidCallback onDataChanged;

  const DataListScreen({
    super.key,
    required this.database,
    required this.onDataChanged,
  });

  @override
  State<DataListScreen> createState() => _DataListScreenState();
}

class _DataListScreenState extends State<DataListScreen> {
  List<StudentData> _scannedStudents = [];
  List<StudentData> _unscannedStudents = [];
  bool _isLoading = true;
  String _errorMessage = '';

  int _scannedCount = 0;
  int _unscannedCount = 0;
  int _totalCount = 0;

  @override
  void initState() {
    super.initState();
    _fetchData();
  }

  Future<void> _fetchData() async {
    setState(() {
      _isLoading = true;
      _errorMessage = '';
    });
    try {
      final List<Map<String, dynamic>> scannedMaps = await widget.database
          .query(
            'students',
            where: 'hasScanned = ?',
            whereArgs: [1],
            orderBy: 'scanTime DESC',
          );
      final List<Map<String, dynamic>> unscannedMaps = await widget.database
          .query(
            'students',
            where: 'hasScanned = ?',
            whereArgs: [0],
            orderBy: 'full_data ASC',
          );

      final List<Map<String, dynamic>> allStudentsMaps = await widget.database
          .query('students');

      setState(() {
        _scannedStudents = scannedMaps
            .map((map) => StudentData.fromMap(map))
            .toList();
        _unscannedStudents = unscannedMaps
            .map((map) => StudentData.fromMap(map))
            .toList();

        _scannedCount = _scannedStudents.length;
        _unscannedCount = _unscannedStudents.length;
        _totalCount = allStudentsMaps.length;

        _isLoading = false;
      });
    } catch (e) {
      debugPrint('Error fetching data: $e');
      setState(() {
        _isLoading = false;
        _errorMessage = 'Failed to load data: $e';
      });
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('Error loading data: $e')));
      }
    }
  }

  Future<void> _resetScanStatus(int studentId) async {
    try {
      await widget.database.update(
        'students',
        {'hasScanned': 0, 'scanTime': null},
        where: 'id = ?',
        whereArgs: [studentId],
      );
      widget.onDataChanged();
      await _fetchData();
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Scan status reset successfully.')),
        );
      }
    } catch (e) {
      debugPrint('Error resetting scan status: $e');
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Failed to reset scan status: $e')),
        );
      }
    }
  }

  Future<void> _deleteAllData() async {
    final bool? confirm = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Confirm Delete All'),
        content: const Text(
          'Are you sure you want to delete ALL student data? This cannot be undone.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(false),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(true),
            style: TextButton.styleFrom(foregroundColor: Colors.red),
            child: const Text('Delete All'),
          ),
        ],
      ),
    );

    if (confirm == true) {
      try {
        await widget.database.delete('students');
        widget.onDataChanged();
        await _fetchData();
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('All data deleted successfully.')),
          );
        }
      } catch (e) {
        debugPrint('Error deleting all data: $e');
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Failed to delete all data: $e')),
          );
        }
      }
    }
  }

  Future<void> _resetAllScanStatuses() async {
    final bool? confirm = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Confirm Reset All Scans'),
        content: const Text(
          'Are you sure you want to reset the scanned status for ALL students?',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(false),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(true),
            child: const Text('Reset All'),
          ),
        ],
      ),
    );

    if (confirm == true) {
      try {
        await widget.database.update(
          'students',
          {'hasScanned': 0, 'scanTime': null},
          where: 'hasScanned = ?',
          whereArgs: [1],
        );
        widget.onDataChanged();
        await _fetchData();
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text('All scanned statuses reset successfully.'),
            ),
          );
        }
      } catch (e) {
        debugPrint('Error resetting all scan statuses: $e');
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Failed to reset all scan statuses: $e')),
          );
        }
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return DefaultTabController(
      length: 2,
      child: Scaffold(
        appBar: AppBar(
          title: const Text('Student Data List'),
          actions: [
            PopupMenuButton<String>(
              onSelected: (value) {
                if (value == 'reset_all_scans') {
                  _resetAllScanStatuses();
                } else if (value == 'delete_all_data') {
                  _deleteAllData();
                }
              },
              itemBuilder: (BuildContext context) => <PopupMenuEntry<String>>[
                const PopupMenuItem<String>(
                  value: 'reset_all_scans',
                  child: Text('Reset All Scanned Statuses'),
                ),
                const PopupMenuItem<String>(
                  value: 'delete_all_data',
                  child: Text('Delete All Data (DANGER)'),
                ),
              ],
            ),
          ],
          bottom: PreferredSize(
            preferredSize: const Size.fromHeight(80.0),
            child: Column(
              children: [
                Padding(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 16.0,
                    vertical: 8.0,
                  ),
                  child: _isLoading
                      ? const Text('Loading counts...')
                      : Row(
                          mainAxisAlignment: MainAxisAlignment.spaceAround,
                          children: [
                            Column(
                              children: [
                                const Text(
                                  'Total',
                                  style: TextStyle(color: Colors.white70),
                                ),
                                Text(
                                  '$_totalCount',
                                  style: const TextStyle(
                                    color: Colors.white,
                                    fontSize: 18,
                                    fontWeight: FontWeight.bold,
                                  ),
                                ),
                              ],
                            ),
                            Column(
                              children: [
                                const Text(
                                  'Scanned',
                                  style: TextStyle(color: Colors.white70),
                                ),
                                Text(
                                  '$_scannedCount',
                                  style: const TextStyle(
                                    color: Colors.white,
                                    fontSize: 18,
                                    fontWeight: FontWeight.bold,
                                  ),
                                ),
                              ],
                            ),
                            Column(
                              children: [
                                const Text(
                                  'Unscanned',
                                  style: TextStyle(color: Colors.white70),
                                ),
                                Text(
                                  '$_unscannedCount',
                                  style: const TextStyle(
                                    color: Colors.white,
                                    fontSize: 18,
                                    fontWeight: FontWeight.bold,
                                  ),
                                ),
                              ],
                            ),
                          ],
                        ),
                ),
                const TabBar(
                  tabs: [
                    Tab(text: 'Scanned'),
                    Tab(text: 'Unscanned'),
                  ],
                ),
              ],
            ),
          ),
        ),
        body: _isLoading
            ? const Center(child: CircularProgressIndicator())
            : _errorMessage.isNotEmpty
            ? Center(
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Text(_errorMessage, textAlign: TextAlign.center),
                    const SizedBox(height: 8),
                    ElevatedButton(
                      onPressed: _fetchData,
                      child: const Text('Retry'),
                    ),
                  ],
                ),
              )
            : TabBarView(
                children: [
                  _buildStudentList(_scannedStudents, isScannedList: true),
                  _buildStudentList(_unscannedStudents, isScannedList: false),
                ],
              ),
      ),
    );
  }

  Widget _buildStudentList(
    List<StudentData> students, {
    required bool isScannedList,
  }) {
    if (students.isEmpty) {
      return Center(
        child: Text(
          isScannedList
              ? 'No students have been scanned yet.'
              : 'All students have been scanned!',
          style: const TextStyle(fontSize: 16, color: Colors.grey),
          textAlign: TextAlign.center,
        ),
      );
    }

    return ListView.builder(
      itemCount: students.length,
      itemBuilder: (context, index) {
        final student = students[index];
        return Card(
          margin: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
          color: isScannedList ? Colors.green.shade50 : Colors.blue.shade50,
          child: ListTile(
            title: Text(
              student.fullData,
              style: const TextStyle(fontWeight: FontWeight.bold),
            ),
            subtitle: student.scanTime != null
                ? Text('Scanned at: ${student.scanTime}')
                : const Text(''),
          ),
        );
      },
    );
  }
}
