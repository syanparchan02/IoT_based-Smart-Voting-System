import 'package:flutter/material.dart';
import 'package:iot_voting_system/screens/data_lists_screen.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:sqflite/sqflite.dart';
import 'package:path/path.dart' show join;
import 'package:path_provider/path_provider.dart';
import 'package:http/http.dart' as http;
import 'package:flutter_tts/flutter_tts.dart';

import 'screens/add_data_screen.dart';

import 'models/student_data.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'QR Code DB Check',
      theme: ThemeData(
        primarySwatch: Colors.teal,
        brightness: Brightness.light,
        useMaterial3: true,
        appBarTheme: AppBarTheme(
          backgroundColor: Colors.teal.shade700,
          foregroundColor: Colors.white,
        ),
      ),
      home: const QRScannerScreen(),
      debugShowCheckedModeBanner: false,
    );
  }
}

class QRScannerScreen extends StatefulWidget {
  const QRScannerScreen({super.key});

  @override
  State<QRScannerScreen> createState() => _QRScannerScreenState();
}

class _QRScannerScreenState extends State<QRScannerScreen> {
  final MobileScannerController controller = MobileScannerController();
  final FlutterTts flutterTts = FlutterTts();

  Database? _db;
  bool _isScanned = false;
  bool _isDbReady = false;
  bool _torchOn = false;

  @override
  void initState() {
    super.initState();
    _initDatabase();
  }

  Future<void> _initDatabase() async {
    try {
      final docDir = await getApplicationDocumentsDirectory();
      final path = join(docDir.path, 'students.db');

      debugPrint('Opening database at: $path');

      _db = await openDatabase(
        path,
        version: 1,
        onCreate: (db, version) async {
          debugPrint('Creating table students...');
          await db.execute('''
            CREATE TABLE students (
              id INTEGER PRIMARY KEY AUTOINCREMENT,
              full_data TEXT UNIQUE,
              hasScanned INTEGER DEFAULT 0,
              scanTime TEXT
            )
          ''');

          debugPrint('Inserting initial data...');
          // Add your  data here
          await db.insert('students', {
            'full_data': 'Mg Hein Thu Soe UCSTT(24-25)-001',
            'hasScanned': 0,
            'scanTime': null,
          });
          await db.insert('students', {
            'full_data': 'Ma Yadanar Aung UCSTT(24-25)-002',
            'hasScanned': 0,
            'scanTime': null,
          });
          await db.insert('students', {
            'full_data': 'Naw Aie Thar Khu UCSTT(24-25)-003',
            'hasScanned': 0,
            'scanTime': null,
          });

          debugPrint('All initial data inserted.');
        },
        onUpgrade: (db, oldVersion, newVersion) async {
          debugPrint(
            'Upgrading database from version $oldVersion to $newVersion',
          );
        },
      );

      setState(() {
        _isDbReady = true;
      });

      debugPrint('Database is ready.');
    } catch (e) {
      debugPrint('Database init error: $e');
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Error initializing database: $e')),
        );
      }
    }
  }

  Future<void> _speak(String text) async {
    await flutterTts.setLanguage("en-US");
    await flutterTts.setPitch(1.0);
    await flutterTts.setSpeechRate(0.5);
    await flutterTts.speak(text);
  }

  Future<bool?> _checkNodeMcuReadiness(BuildContext context) async {
    // ⚠️ IMPORTANT: Replace with your actual NodeMCU IP address and a dedicated status endpoint
    // This endpoint should *only* return status, NOT open the door.
    const nodeMcuStatusUrl = 'http://172.20.10.4/status'; // <--- MODIFIED URL

    debugPrint(
      'Attempting to contact NodeMCU for status at: $nodeMcuStatusUrl',
    );
    try {
      final response = await http
          .get(Uri.parse(nodeMcuStatusUrl))
          .timeout(const Duration(seconds: 5)); // Added a timeout

      debugPrint(
        '📡 NodeMCU status responded: ${response.statusCode} ${response.body}',
      );

      if (response.statusCode == 200) {
        // NodeMCU is logged in and ready
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('✅ NodeMCU is ready: ${response.body}')),
          );
        }
        return true; // NodeMCU is active and logged in
      } else if (response.statusCode == 403) {
        // NodeMCU requires login (Access Denied: Not logged in.)
        if (mounted) {
          _speak('System is not open');
          _showDialog(
            'System is not open yet because it not time to vote',
            false, // Is not a success
            resetScan: true, // Allow scanner to restart after this dialog
          );
        }
        return false; // NodeMCU not logged in
      } else {
        // Other unexpected status codes
        if (mounted) {
          _speak('Failed to get NodeMCU readiness response');
          _showDialog(
            '❌ Failed to get readiness response from NodeMCU: ${response.statusCode}\n${response.body}',
            false,
            resetScan: true, // Allow scanner to restart after this dialog
          );
        }
        return null; // Unexpected status
      }
    } catch (e) {
      // Network error (e.g., NodeMCU unreachable, incorrect IP, timeout)
      debugPrint('HTTP error connecting to NodeMCU status endpoint: $e');
      if (mounted) {
        _speak('Fail to connect, Please check the internet again');
        _showDialog(
          '⚠️ Fail to connect, Please check the internet again',
          false,
          resetScan: true, // Allow scanner to restart after this dialog
        );
      }
      return null; // Network error
    }
  }

  /// Sends a command to NodeMCU to open the door.
  /// This function should ONLY be called when the door needs to open.
  /// Returns `true` on success (HTTP 200), `false` otherwise.
  Future<bool> _triggerNodeMcuDoorOpen() async {
    // ⚠️ IMPORTANT: Replace with your actual NodeMCU IP address and the dedicated door open endpoint
    const nodeMcuDoorOpenUrl =
        'http://172.20.10.4/open_door'; // <--- NEW URL FOR OPENING DOOR

    debugPrint(
      'Attempting to trigger NodeMCU door open at: $nodeMcuDoorOpenUrl',
    );
    try {
      final response = await http
          .get(Uri.parse(nodeMcuDoorOpenUrl))
          .timeout(const Duration(seconds: 5));

      debugPrint(
        '🚪 NodeMCU door open response: ${response.statusCode} ${response.body}',
      );

      if (response.statusCode == 200) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text('✅ Door opened by NodeMCU: ${response.body}'),
            ),
          );
        }
        return true;
      } else {
        // NodeMCU responded but not with 200 OK for door open
        if (mounted) {
          _speak('Failed to open door');
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text(
                '❌ NodeMCU failed to open door: ${response.statusCode} ${response.body}',
              ),
            ),
          );
        }
        return false;
      }
    } catch (e) {
      // Network error during door open attempt
      debugPrint('HTTP error sending door open command to NodeMCU: $e');
      if (mounted) {
        _speak('Error sending door command');
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('⚠️ Error sending door command: $e')),
        );
      }
      return false;
    }
  }

  Future<void> _onDetectBarcode(BarcodeCapture capture) async {
    // Prevent multiple scans or processing if database is not ready or already processing
    if (_isScanned || !_isDbReady || _db == null) return;

    // Ensure a barcode was actually captured
    if (capture.barcodes.isEmpty) return;

    final barcode = capture.barcodes.first;
    final String? result = barcode.rawValue;

    if (result != null && mounted) {
      final scannedText = result.trim().replaceAll('\n', ' ');
      debugPrint('Scanned text: "$scannedText"');

      // Set _isScanned to true to prevent re-triggering and stop the scanner
      setState(() {
        _isScanned = true;
      });
      controller.stop(); // Stop scanner immediately after detection

      // --- Step 1: Check NodeMCU Readiness (without opening door) ---
      debugPrint('Step 1: Checking NodeMCU readiness...');
      final bool? nodeMcuReady = await _checkNodeMcuReadiness(context);

      if (!mounted) return;

      // If NodeMCU is NOT ready or communication failed, show relevant dialog and stop.
      // _checkNodeMcuReadiness already shows a dialog and manages scanner restart if needed.
      if (nodeMcuReady == false || nodeMcuReady == null) {
        debugPrint(
          'NodeMCU not ready or connection error. Skipping database check and door open.',
        );
        return; // Exit, _checkNodeMcuReadiness handles the UI feedback.
      }

      // --- Step 2: If NodeMCU is Ready, proceed with Database Check ---
      debugPrint('Step 2: NodeMCU is ready. Proceeding with database check.');
      final matches = await _db!.query(
        'students',
        where: 'full_data = ?',
        whereArgs: [scannedText],
      );

      if (!mounted) return;

      if (matches.isNotEmpty) {
        final student = matches.first;

        // Check if already scanned
        if ((student['hasScanned'] ?? 0) == 1) {
          // Already scanned: Show "Close" button. DO NOT NOTIFY NodeMCU TO OPEN DOOR.
          _showDialog(
            '$scannedText\n\n⚠️ Already Scanned.',
            false,
            resetScan: true,
          );
          await _speak('Already scanned');
          debugPrint('Student already scanned. Door NOT opened.');
        } else {
          // Valid, first-time scan: Update database. THEN, NOTIFY NodeMCU TO OPEN DOOR.
          final now = DateTime.now().toIso8601String();
          await _db!.update(
            'students',
            {'hasScanned': 1, 'scanTime': now},
            where: 'id = ?',
            whereArgs: [student['id']],
          );
          debugPrint('Student found and updated. Attempting to open door...');

          // --- Step 3: Trigger NodeMCU to open door ONLY if database check is successful ---
          final bool doorOpened = await _triggerNodeMcuDoorOpen();

          if (!mounted) return;

          if (doorOpened) {
            _showDialog(
              '$scannedText\n\n✅ Now you can vote',
              true,
              resetScan: true,
            );
            await _speak('Success, Now you can vote');
          } else {
            // Door failed to open (e.g., NodeMCU side issue, although it was 'ready')
            _showDialog(
              '"$scannedText\n\n❗ Database updated, but door failed to open.',
              false,
              resetScan: true,
            );
            await _speak('Door not opened');
          }
        }
      } else {
        // Not found in database: Show "Close" button. DO NOT NOTIFY NodeMCU TO OPEN DOOR.
        _showDialog(
          '$scannedText\n\n❌ You are not on the voting list',
          false,
          resetScan: true,
        );
        await _speak('You are not on the voting list');
        debugPrint('Student not found in database. Door NOT opened.');
      }
    }
    // _isScanned is reset and controller restarted inside _showDialog's OK/Close button press
  }

  // Modified _showDialog to dynamically show "OK" or "Close" and handle scan reset
  void _showDialog(String message, bool isSuccess, {bool resetScan = true}) {
    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (ctx) => AlertDialog(
        title: Text(
          isSuccess ? 'Success' : 'Error',
          style: TextStyle(color: isSuccess ? Colors.green : Colors.red),
        ),
        content: Text(message, style: const TextStyle(fontSize: 18)),
        actions: [
          TextButton(
            onPressed: () {
              Navigator.of(ctx).pop();
              if (mounted && resetScan) {
                setState(() {
                  _isScanned =
                      false; // Allow re-scanning after dialog is dismissed
                });
                controller.start(); // Restart scanner after dialog
              }
            },
            // Dynamically set button text
            child: Text(
              isSuccess ? 'OK' : 'Close',
              style: const TextStyle(fontSize: 16),
            ),
          ),
        ],
      ),
    );
  }

  void _openDataListScreen() async {
    if (_isDbReady && _db != null) {
      controller.stop(); // Stop scanner before navigating

      await Navigator.of(context).push(
        MaterialPageRoute(
          builder: (context) => DataListScreen(
            database: _db!,
            onDataChanged: () {
              debugPrint(
                'Data changed in DataListScreen. Refresh UI if needed.',
              );
              // The DataListScreen handles its own refresh on data changes.
              // If you had a summary on the main screen that needed updating, you'd put that logic here.
            },
          ),
        ),
      );

      if (mounted) {
        setState(() {
          _isScanned =
              false; // Reset scan state after returning from another screen
        });
        controller.start(); // Restart scanner
      }
    } else {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Database not ready yet.')),
        );
      }
    }
  }

  void _openAddDataScreen() async {
    if (_isDbReady && _db != null) {
      controller.stop(); // Stop scanner before navigating

      // The pop result will be true if data was added, null otherwise
      final bool? dataAdded = await Navigator.of(context).push(
        MaterialPageRoute(builder: (context) => AddDataScreen(database: _db!)),
      );

      if (mounted) {
        setState(() {
          _isScanned =
              false; // Reset scan state after returning from another screen
        });
        controller.start(); // Restart scanner

        if (dataAdded == true) {
          debugPrint('New data added from AddDataScreen.');
          // If you had a summary of student counts on this screen, you could update it here.
          // Otherwise, the DataListScreen will fetch the new data when it's opened again.
        }
      }
    } else {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Database not ready yet.')),
        );
      }
    }
  }

  @override
  void dispose() {
    controller.dispose();
    _db?.close(); // Close the database connection
    flutterTts.stop(); // Stop any ongoing TTS
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('QR Code Reader'),
        actions: [
          // ❌ Removed switch camera to always use front
          IconButton(
            tooltip: 'Add new data',
            icon: const Icon(Icons.add_box_outlined),
            onPressed: _openAddDataScreen,
          ),
          IconButton(
            tooltip: 'View all data',
            icon: const Icon(Icons.list_alt),
            onPressed: _openDataListScreen,
          ),
          IconButton(
            tooltip: 'Switch camera',
            icon: const Icon(Icons.cameraswitch),
            onPressed: () => controller.switchCamera(),
          ),
        ],
      ),
      body: _isDbReady
          ? Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  const Text(
                    'Welcome to IOT Voting System',
                    style: TextStyle(fontSize: 20),
                  ),
                  const SizedBox(height: 40),
                  AspectRatio(
                    aspectRatio: 1.0,
                    child: ClipRRect(
                      borderRadius: BorderRadius.circular(16),
                      child: MobileScanner(
                        controller: controller,
                        onDetect: _onDetectBarcode,
                      ),
                    ),
                  ),
                  const SizedBox(height: 24),
                  if (_isScanned)
                    Padding(
                      padding: const EdgeInsets.only(top: 24),
                      child: Row(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: const [
                          CircularProgressIndicator(),
                          SizedBox(width: 12),
                          Text(
                            'Processing scan...',
                            style: TextStyle(fontSize: 16),
                          ),
                        ],
                      ),
                    ),
                  if (!_isScanned && _isDbReady)
                    const Padding(
                      padding: EdgeInsets.only(top: 24),
                      child: Text(
                        'Point your camera at a QR code to scan.\nEnsure good lighting.',
                        style: TextStyle(fontSize: 16, color: Colors.grey),
                        textAlign: TextAlign.center,
                      ),
                    ),
                ],
              ),
            )
          : const Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  CircularProgressIndicator(),
                  SizedBox(height: 16),
                  Text(
                    'Initializing database...',
                    style: TextStyle(fontSize: 16),
                  ),
                ],
              ),
            ),
    );
  }
}
