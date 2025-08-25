class StudentData {
  final int? id;
  final String fullData;
  final bool hasScanned;
  final String? scanTime;

  StudentData({
    this.id,
    required this.fullData,
    this.hasScanned = false,
    this.scanTime,
  });

  factory StudentData.fromMap(Map<String, dynamic> map) {
    return StudentData(
      id: map['id'] as int,
      fullData: map['full_data'] as String,
      hasScanned: (map['hasScanned'] as int) == 1,
      scanTime: map['scanTime'] as String?,
    );
  }

  Map<String, dynamic> toMap() {
    return {
      'id': id,
      'full_data': fullData,
      'hasScanned': hasScanned ? 1 : 0,
      'scanTime': scanTime,
    };
  }
}
