import dm_testbench

"""Class collects unit tests for flow.
Test the dm-cmd flow command with combinations of -a and -l
"""
class UnitTestFlowpattern(dm_testbench.DmTestbench):

  def common_dynamic_branch_single_tvalid(self, delay=0.0, options=[]):
    self.startPattern('dynamic-branch-single-schedule.dot', 'IN_C0')
    stdoutLines = self.startAndGetSubprocessStdout((self.binary_dm_sched, self.datamaster, 'rawvisited'))
    self.compareExpectedOutput(stdoutLines, self.schedules_folder + 'dynamic-branch-single-expected-0-0.txt')
    argumentList = [self.binary_dm_cmd, self.datamaster, 'flowpattern', 'IN_C0', 'B']
    argumentList.extend(options)
    self.startAndCheckSubprocess(argumentList)
    if delay > 0.0:
      self.delay(delay)
    stdoutLines = self.startAndGetSubprocessStdout((self.binary_dm_cmd, self.datamaster, 'rawqueue', 'BLOCK_IN0'))
    self.compareExpectedOutput(stdoutLines, self.schedules_folder + 'dynamic-branch-single-expected-1-1.txt', excludeField='VTIME:')
    self.startAndCheckSubprocess((self.binary_dm_cmd, self.datamaster, 'startpattern', 'IN_C0'))
    stdoutLines = self.startAndGetSubprocessStdout((self.binary_dm_sched, self.datamaster, 'rawvisited'))
    self.compareExpectedOutput(stdoutLines, self.schedules_folder + 'dynamic-branch-single-expected-1-3.txt')
    stdoutLines = self.startAndGetSubprocessStdout((self.binary_dm_cmd, self.datamaster, 'rawqueue', 'BLOCK_IN0'))
    self.compareExpectedOutput(stdoutLines, self.schedules_folder + 'dynamic-branch-single-expected-1-4.txt', excludeField='VTIME:')

  def test_dynamic_branch_single_tvalid_rel_0(self):
    """Load the schedule dynamic-branch-single-schedule.dot and start pattern IN_C0.
    Check that nodes BLOCK_A and BLOCK_IN0 are visited.
    Execute a flowpattern command, which is immediatley valid, to change flow from pattern A to B in block BLOCK_IN0.
    Wait for 0.4 seconds. Check the queues of BLOCK_IN0, the low prio queue should contain the flow command.
    Start pattern IN_C0. Check that all three nodes are visited.
    Check the queues of BLOCK_IN0, the low prio queue should contain the executed flow command.
    """
    # ~ does not work with a delay of 0.0 seconds
    self.common_dynamic_branch_single_tvalid(delay=0.4)

  def test_dynamic_branch_single_tvalid_rel_1(self):
    """Load the schedule dynamic-branch-single-schedule.dot and start pattern IN_C0.
    Check that nodes BLOCK_A and BLOCK_IN0 are visited.
    Execute a flowpattern command, which is valid after 1 second, to change flow from pattern A to B in block BLOCK_IN0.
    Wait for 1.4 seconds. Check the queues of BLOCK_IN0, the low prio queue should contain the flow command.
    Start pattern IN_C0. Check that all three nodes are visited.
    Check the queues of BLOCK_IN0, the low prio queue should contain the executed flow command.
    """
    self.common_dynamic_branch_single_tvalid(delay=1.4, options=['-l', '1000000000'])

  def test_dynamic_branch_single_tvalid_abs_0(self):
    """Load the schedule dynamic-branch-single-schedule.dot and start pattern IN_C0.
    Check that nodes BLOCK_A and BLOCK_IN0 are visited.
    Execute a flowpattern command, which is valid at an absolute time, to change flow from pattern A to B in block BLOCK_IN0.
    Wait for 1.4 seconds. Check the queues of BLOCK_IN0, the low prio queue should contain the flow command.
    Start pattern IN_C0. Check that all three nodes are visited.
    Check the queues of BLOCK_IN0, the low prio queue should contain the executed flow command.
    """
    self.common_dynamic_branch_single_tvalid(options=['-a'])

  def test_dynamic_branch_single_tvalid_abs_1(self):
    """Load the schedule dynamic-branch-single-schedule.dot and start pattern IN_C0.
    Check that nodes BLOCK_A and BLOCK_IN0 are visited.
    Execute a flowpattern command, which is valid at an absolute time (1 second from now), to change flow from pattern A to B in block BLOCK_IN0.
    Wait for 1.4 seconds. Check the queues of BLOCK_IN0, the low prio queue should contain the flow command.
    Start pattern IN_C0. Check that all three nodes are visited.
    Check the queues of BLOCK_IN0, the low prio queue should contain the executed flow command.
    """
    self.common_dynamic_branch_single_tvalid(delay=1.0, options=['-l', '1000000000', '-a'])
