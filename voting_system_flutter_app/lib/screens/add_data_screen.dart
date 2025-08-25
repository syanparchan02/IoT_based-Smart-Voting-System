import 'package:flutter/material.dart';
import 'package:sqflite/sqflite.dart';
import '../models/student_data.dart';

class AddDataScreen extends StatefulWidget {
  final Database database;

  const AddDataScreen({super.key, required this.database});

  @override
  State<AddDataScreen> createState() => _AddDataScreenState();
}

class _AddDataScreenState extends State<AddDataScreen> {
  final _formKey = GlobalKey<FormState>();
  final TextEditingController _dataController = TextEditingController();

  @override
  void dispose() {
    _dataController.dispose();
    super.dispose();
  }

  void _saveData() async {
    if (_formKey.currentState!.validate()) {
      final newStudent = StudentData(fullData: _dataController.text.trim());

      try {
        await widget.database.insert(
          'students',
          newStudent.toMap(),
          conflictAlgorithm:
              ConflictAlgorithm.ignore, // To prevent duplicate inserts
        );
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Data added successfully!')),
          );
          Navigator.of(context).pop(true);
        }
      } catch (e) {
        debugPrint('Error inserting data: $e');
        String errorMessage = 'Failed to add data.';
        if (e.toString().contains('UNIQUE constraint failed')) {
          errorMessage = 'Error: This data already exists for a student.';
        }
        if (mounted) {
          ScaffoldMessenger.of(
            context,
          ).showSnackBar(SnackBar(content: Text(errorMessage)));
        }
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Add New Student Data')),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Form(
          key: _formKey,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              TextFormField(
                controller: _dataController,
                decoration: const InputDecoration(
                  labelText:
                      'Student Full Data (e.g., Mg Aung Aung UCSTT(24-25)-001)',
                  border: OutlineInputBorder(),
                ),
                validator: (value) {
                  if (value == null || value.trim().isEmpty) {
                    return 'Please enter student data.';
                  }
                  return null;
                },
                maxLines: 3,
                minLines: 1,
              ),
              const SizedBox(height: 20),
              ElevatedButton.icon(
                onPressed: _saveData,
                icon: const Icon(Icons.save),
                label: const Text('Save Data'),
                style: ElevatedButton.styleFrom(
                  padding: const EdgeInsets.symmetric(vertical: 12),
                  textStyle: const TextStyle(fontSize: 18),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
