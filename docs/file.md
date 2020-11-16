## File
File is a class to add I/O capabilities to Gravity.

### Class methods example
```swift
func main() {
  var target_file = "FULL_PATH_TO_A_TEXT_FILE_HERE";
  var target_folder = "FULL_PATH_TO_A_FOLDER_HERE";

  // FILE TEST
  var size = File.size(target_file);
  var exists = File.exists(target_file);
  var is_dir = File.is_directory(target_file);
  var data = File.read(target_file);
	 
  System.print("File: " + target_file);
  System.print("Size: " + size);
  System.print("Exists: " + exists);
  System.print("Is Directory: " + is_dir);
  System.print("Data: " + data);
	 
  // FOLDER TEST
  func closure (file_name, full_path, is_directory) {
    if (is_directory) {
      System.print("+ \(file_name)");
    } else {
      System.print("    \(file_name)");
    }
  }
	 
  var recursive = true;
  var n = File.directory_scan(target_folder, recursive, closure);
	 
  // return the number of file processed
  return n;
}
```

### Read/write buffer example
```swift
func main() {
  var target_file = "FULL_PATH_TO_A_TEXT_FILE_HERE";

  // WRITE TEST
  // 2nd argument to open is the same as the mode argument to the fopen function
  // https://pubs.opengroup.org/onlinepubs/009695399/functions/fopen.html
  var f = File.open(target_file, "w+");

  f.write("This is the first line\n");
  f.write("This is the second line\n");
  f.write("This is the third line\n");

  f.close();

  // READ TEST
  f = File.open(target_file, "r");
  var data = f.read(40);
  f.close();

  return data;
}
```

### Read line example
```
func main() {
    var target_file = "FULL_PATH_TO_A_TEXT_FILE_HERE";
    var f = File.open(target_file, "w+");
    
    f.write("This is the first line\n");
    f.write("This is the second line\n");
    f.write("This is the third line\n");
    
    f.close();
    
    f = File.open(target_file, "r");
    while (!f.isEOF()) {
        var line = f.read("\n");
        System.print(line);
    }
    
    f.close();
    
    return 0;
}
```
