import collections # used in analyseFrequencyFromCsv
import contextlib
import csv         # used in analyseFrequencyFromCsv
import datetime    # used in analyseFrequencyFromCsv
import difflib     # used in compareExpectedResult
import os
import pathlib
import subprocess  # used in startAllPatterns
import sys
import threading
import time
import unittest    # contains super class

"""
Module dm_testbench collects functions to handle patterns for the data master testbench.
"""
class DmTestbench(unittest.TestCase):
  @classmethod
  def setUpClass(self):
    """Set up for all test cases: store the environment variables in variables.
    """
    self.binary_dm_cmd = os.environ.get('TEST_BINARY_DM_CMD', 'dm-cmd')
    self.binary_dm_sched = os.environ.get('TEST_BINARY_DM_SCHED', 'dm-sched')
    self.datamaster = os.environ['DATAMASTER']
    self.schedules_folder = os.environ.get('TEST_SCHEDULES', 'schedules/')
    self.snoop_command = os.environ.get('SNOOP_COMMAND', 'saft-ctl tr0 -xv snoop 0 0 0')
    self.patternStarted = False

  def setUp(self):
    self.initDatamaster()

  def initDatamaster(self):
    """Initialize (clean) the datamaster.
    The data master is halted, cleared, and statistics is reset.
    This is all done with 'dm-cmd reset all'.
    """
    self.startAndCheckSubprocess([self.binary_dm_cmd, self.datamaster, 'reset', 'all'])
    # ~ self.startAndCheckSubprocess([self.binary_dm_cmd, self.datamaster, 'halt'])
    # ~ self.startAndCheckSubprocess([self.binary_dm_sched, self.datamaster, 'clear'])
    # ~ self.startAndCheckSubprocess([self.binary_dm_cmd, self.datamaster, 'cleardiag'])

  def addSchedule(self, schedule_file):
    """Connect to the given data master and load the schedule file (dot format).
    """
    self.startAllPattern(schedule_file, start=False)

  def startPattern(self, schedule_file):
    """Connect to the given data master and load the schedule file (dot format).
    Search for the first pattern in the data master with self.binary_dm_sched and start it.
    """
    self.startAllPattern(schedule_file, '', onePattern=True)

  def startPattern(self, schedule_file, pattern=''):
    """Connect to the given data master and load the schedule file (dot format).
    Start the given pattern.
    """
    self.startAllPattern(schedule_file, pattern, onePattern=False)

  def startAllPattern(self, schedule_file, pattern='', onePattern=False, start=True):
    """Connect to the given data master and load the schedule file (dot format).
    Search for all pattern in the data master with self.binary_dm_sched and start these.
    """
    schedule_file = self.schedules_folder + schedule_file
    print (f"Connect to device '{self.datamaster}', schedule file '{schedule_file}'.   ", end='', flush=True)
    self.startAndCheckSubprocess([self.binary_dm_sched, self.datamaster, 'add', schedule_file])
    if start:
      if len(pattern) > 0:
        self.startAndCheckSubprocess([self.binary_dm_cmd, self.datamaster, 'startpattern', pattern])
      else:
        # run 'dm-sched self.datamaster' as a sub process.
        process = subprocess.Popen([self.binary_dm_sched, self.datamaster], stderr=subprocess.PIPE, stdout=subprocess.PIPE)
        # get command output and error
        stdout, stderr = process.communicate()
        lines = stdout.decode('utf-8').splitlines()
        # start the first pattern found in the data master.
        patterns = False
        for i in range(len(lines)):
          if patterns and len(lines[i]) > 0:
            # print (f"Pattern: \n{lines[i]} {lines} {lines[i].split()[0]}")
            self.startAndCheckSubprocess([self.binary_dm_cmd, self.datamaster, 'startpattern', lines[i].split()[0]])
            if onePattern:
              patterns = False
          if 'Patterns' in lines[i]:
            patterns = True

  def startAndCheckSubprocess(self, argumentsList, expectedReturnCode=[0], linesCout=-1, linesCerr=-1):
    """Common method to start a subprocess and check the return code.
    The <argumentsList> contains the binary to execute and all arguments in one list.
    Start the binary for the test step with the arguments and check the output on stdout and stderr and the return code as well.
    """
    # pass cmd and args to the function
    process = subprocess.Popen([*argumentsList], stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    # get command output and error
    stdout, stderr = process.communicate()
    self.assertTrue(process.returncode in expectedReturnCode, f'wrong return code {process.returncode}, expected: {expectedReturnCode}, '
          + f'Command line: {argumentsList}\nstderr: {stderr.decode("utf-8").splitlines()}\nstdout: {stdout.decode("utf-8").splitlines()}')
    if linesCerr > -1:
      lines = stderr.decode('utf-8').splitlines()
      self.assertEqual(len(lines), linesCerr, f'wrong stderr, expected {linesCerr} lines, Command line: {argumentsList}\nstderr: {lines}\nstdout: {stdout.decode("utf-8").splitlines()}')
    if linesCout > -1:
      lines = stdout.decode('utf-8').splitlines()
      self.assertEqual(len(lines), linesCout, f'wrong stdout, expected {linesCout} lines, Command line: {argumentsList}\nstderr: {stderr.decode("utf-8").splitlines()}\nstdout: {lines}')

  def startAndGetSubprocessStdout(self, argumentsList, expectedReturnCode=[0], linesCout=-1, linesCerr=-1):
    return self.startAndGetSubprocessOutput(argumentsList, expectedReturnCode, linesCout, linesCerr)[0]

  def startAndGetSubprocessOutput(self, argumentsList, expectedReturnCode=[-1], linesCout=-1, linesCerr=-1):
    """Common method to start a subprocess and check the return code.
    The <argumentsList> contains the binary to execute and all arguments in one list.
    Start the binary for the test step with the arguments and check the number of lines for stdout and stderr.
    Check that the return code is in a list of allowed return codes.
    Return both stdout, stderr as list of lines.
    """
    # pass cmd and args to the function
    process = subprocess.Popen([*argumentsList], stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    # get command output and error
    stdout, stderr = process.communicate()
    self.assertTrue(process.returncode in expectedReturnCode, f'wrong return code {process.returncode}, expected: {expectedReturnCode}, '
          + f'Command line: {argumentsList}\nstderr: {stderr.decode("utf-8").splitlines()}\nstdout: {stdout.decode("utf-8").splitlines()}')
    if linesCerr > -1:
      lines = stderr.decode('utf-8').splitlines()
      self.assertEqual(len(lines), linesCerr, f'wrong stderr, expected {linesCerr} lines, Command line: {argumentsList}\nstderr: {lines}\nstdout: {stdout.decode("utf-8").splitlines()}')
    if linesCout > -1:
      lines = stdout.decode('utf-8').splitlines()
      self.assertEqual(len(lines), linesCout, f'wrong stdout, expected {linesCout} lines, Command line: {argumentsList}\nstderr: {stderr.decode("utf-8").splitlines()}\nstdout: {lines}')
    return [stdout.decode("utf-8").splitlines(), stderr.decode("utf-8").splitlines()]

  def removePaintedFlags(self, dot_lines):
    """Remove flags and fillcolors of painted nodes from dot string (parameter dot_lines).
    """
    for index, line in enumerate(dot_lines):
      dot_lines[index] = line.replace('flags="0x00000107"', 'flags="0x00000007"') \
      .replace('flags="0x00002107"', 'flags="0x00002007"') \
      .replace('flags="0x00108107"', 'flags="0x00108007"') \
      .replace('flags="0x00708107"', 'flags="0x00708007"') \
      .replace('flags="0x00100107"', 'flags="0x00100007"') \
      .replace('fillcolor = "green"', 'fillcolor = "white"') \
      .replace('flags="0x00020007"', 'flags="0x00000007"') \
      .replace('flags="0x00022007"', 'flags="0x00002007"') \
      .replace('flags="0x00120007"', 'flags="0x00100007"') \
      .replace('flags="0x00128007"', 'flags="0x00108007"') \
      .replace(', fontname="Times-Bold", fontcolor = "blue2", fontsize="16"', '')
    return dot_lines

  def compareExpectedResult(self, file_current, file_expected, exclude=''):
    """Compare a file with a test result with an expected result contained in <file_expected>.
    Lines with the string <exclude> are removed from <file_current> before checking against <file_expected>.
    Assert that a unified diff has no lines.
    """
    with open(file_current, 'r') as f_current:
      current = f_current.readlines()
    if len(exclude) > 0:
      current = [ x for x in current if exclude not in x ]
    current = self.removePaintedFlags(current)
    with open(file_expected, 'r') as f_expected:
      expected = f_expected.readlines()
    diffLines = list(difflib.unified_diff(current, expected, n=0))
    self.assertEqual(len(diffLines), 0, f'Diff: file {file_expected}\n{diffLines}')

  def compareExpectedOutput(self, output, file_expected, exclude='', excludeField='', delete=[]):
    """Compare an output with a test result with an expected result contained in <file_expected>.
    1. Lines with the string <exclude> are removed from <output> before checking against <file_expected>.
    2. Lines in the list 'delete' are deleted in <output> and <file_expected>.
    3. Lines which contain <excludeField> are not compared after the string specified in 'excludeField'.
    Assert that a unified diff has no lines.
    """
    current = output
    if len(exclude) > 0:
      current = [ x for x in current if exclude not in x ]
    for index in delete:
      del current[index]
    if len(excludeField) > 0:
      for line in current:
        index = current.index(line)
        pos = line.find(excludeField)
        if pos > -1:
          current[index] = line[:(pos + len(excludeField))]
    current = self.removePaintedFlags(current)
    # ~ with open('compare-current.txt', 'w') as file1:
      # ~ file1.write("\n".join(current))
    with open(file_expected, 'r') as f_expected:
      expected = f_expected.read().splitlines()
    for index in delete:
      del expected[index]
    if len(excludeField) > 0:
      for line in expected:
        index = expected.index(line)
        pos = line.find(excludeField)
        if pos > -1:
          expected[index] = line[:(pos + len(excludeField))]
    # ~ with open('compare-expected.txt', 'w') as file2:
      # ~ file2.write("\n".join(expected))
    diffLines = list(difflib.unified_diff(current, expected, n=0))
    self.assertEqual(len(diffLines), 0, f'Diff: file {file_expected}\n{diffLines}')

  def getSnoopCommand(self, duration):
    snoop_command1 = self.snoop_command + ' ' + str(duration)
    if self.snoop_command[len(self.snoop_command)-1:] == "'":
      snoop_command1 = self.snoop_command[:-1] + ' ' + str(duration) + "'"
    # ~ print(f'getSnoopCommand: {snoop_command1}')
    return snoop_command1

  def snoopToCsv(self, csv_file_name, duration=1):
    """Snoop timing messages with saft-ctl for <duration> seconds (default = 1) and write the messages to <csv_file_name>.
    Details: start saft-ctl with Popen, run it for <duration> seconds.
    """
    with open(csv_file_name, 'wb') as file1:
      process = subprocess.run(self.getSnoopCommand(duration), shell=True, check=True, stdout=file1)
      self.assertEqual(process.returncode, 0, f'Returncode: {process.returncode}')

  def snoopToCsvWithAction(self, csv_file_name, action, duration=1):
    """Snoop timing messages with saft-ctl for <duration> seconds (default = 1).
    Write the messages to <csv_file_name>.
    Details: start saft-ctl with Popen in its own thread, run it for <duration> seconds.
    action should end before snoop.
    """
    snoop = threading.Thread(target=self.snoopToCsv, args=(csv_file_name, duration))
    snoop.start()
    action()
    snoop.join()

  def analyseFrequencyFromCsv(self, csv_file_name, column=20, printTable=True):
    """Analyse the frequency of the values in the specified column. Default column is 20 (parameter of the timing message).
    Prints (if printtable=True) the table of values, counters, and frequency over the whole time span.
    """
    line_count = 0
    maxTime = datetime.datetime.strptime("2000-01-01", '%Y-%m-%d')
    minTime = datetime.datetime.strptime("2500-01-01", '%Y-%m-%d')
    listParam = []
    # Read csv_file_name as csv and collect the column in listParam.
    # Read the message timestamp from column 1 and 2, precision: microseconds, not nanoseconds.
    with open(csv_file_name) as csv_file:
      csv_reader = csv.reader(csv_file, delimiter=' ')
      for row in csv_reader:
        line_count += 1
        time1 = datetime.datetime.strptime(row[1] + ' ' + row[2][:-3], '%Y-%m-%d %H:%M:%S.%f')
        if minTime > time1:
          minTime = time1
        if time1 > maxTime:
          maxTime = time1
        listParam.append(row[column])
      timeSpan = (maxTime-minTime).total_seconds()
      listCounter = sorted(collections.Counter(listParam).items())
      if printTable:
        maxLengthKey = len("Value")
        maxLengthValue = len("Count")
        if len(str(line_count)) > maxLengthValue:
          maxLengthValue = len(str(line_count))
        alignKeys = False
        for key, value in listCounter:
          if len(key) > maxLengthKey:
            maxLengthKey = len(key)
          if len(str(value)) > maxLengthValue:
            maxLengthValue = len(str(value))
          if "!delayed" in key or "!conflict" in key:
            alignKeys = True
        print()
        print(f'{"Value":^{maxLengthKey+1}s}  {"Count":>{maxLengthValue}s}   {"Frequency":>9s}')
        for key, value in listCounter:
          keyAligned = key
          if "!" not in key and alignKeys:
            keyAligned = key + "         "
          if "!delayed" in key:
            keyAligned = key + " "
          print(f'{keyAligned:>{maxLengthKey + 1}s}: {value:{maxLengthValue}d} {value/timeSpan:9.3f}Hz')
        print(f'{"All":>{maxLengthKey + 1}s}: {line_count:{maxLengthValue}d} {line_count/timeSpan: >9.3f}Hz, time span: {timeSpan:0.6f}sec')

  def delay(self, duration):
    """Delay for <duration> seconds.
    """
    time.sleep(duration)

  def deleteFile(self, fileName):
    """Delete file <fileName>.
    """
    fileToRemove = pathlib.Path(fileName)
    if fileToRemove.exists():
      fileToRemove.unlink()
