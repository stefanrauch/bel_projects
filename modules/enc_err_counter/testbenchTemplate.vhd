--wishbone slave testbench
--bare bones testbench to test slave communication

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
library std;
use std.textio.all;
library work;
use work.wishbone_pkg.all;


--Entity(empty)
entity enc_err_counter_tb is
end;

architecture enc_err_counter_tb_arc of enc_err_counter_tb is
	--Testbench settings
	constant c_reset_time  		: time    := 190 ns;
	constant c_sys_clock_cycle 	: time    := 16.1 ns;
	constant c_ref_clock_cycle  : time 	  := 7.9 ns;
	
	-- Other constants
	constant c_cyc_on                      : std_logic := '1';
	constant c_cyc_off                     : std_logic := '0';
	constant c_str_on                      : std_logic := '1';
	constant c_str_off                     : std_logic := '0';
	constant c_we_on                       : std_logic := '1';
	constant c_we_off                      : std_logic := '0';
	constant c_reg_all_zero				   : std_logic_vector(31 downto 0) := x"00000000";
	
	-- Basic device signals
	signal s_clk_sys	: std_logic := '0';
	signal s_clk_ref	: std_logic := '0';
	signal s_rst_n 		: std_logic := '0';
	signal s_rst   		: std_logic := '0';
	
	-- Wishbone connections
	signal s_wb_slave_in  : t_wishbone_slave_in;
	signal s_wb_slave_out : t_wishbone_slave_out;
	signal s_wb_desc_out  : t_wishbone_device_descriptor;
	
	-- Testbench logic
	signal s_sequence_cnt   : std_logic_vector(31 downto 0);
	signal s_ack            : std_logic;
	signal s_flag			: std_logic;
	signal enc_err			: std_logic := '0';
	
	-- Functions
	-- Function wb_stim -> Helper function to create a human-readable testbench
	function wb_stim(cyc : std_logic; stb : std_logic; we : std_logic;
		             adr : t_wishbone_address; dat : t_wishbone_data) return t_wishbone_slave_in is
	variable v_setup : t_wishbone_slave_in;
	begin
		v_setup.cyc := cyc;
		v_setup.stb := stb;
		v_setup.we  := we;
		v_setup.adr := adr;
		v_setup.dat := dat;
		v_setup.sel := (others => '0'); -- Don't care
		return v_setup;
	end function wb_stim;
	
	-- Procedures
	-- Procedure wb_expect -> Check WB slave answer
	procedure wb_expect(msg : string; dat_from_slave : t_wishbone_data; compare_value : t_wishbone_data) is
	begin
		if (to_integer(unsigned(dat_from_slave)) = to_integer(unsigned(compare_value))) then
		  report "Test passed: " & msg;
		else
		  report "Test errored: " & msg;
		  report "-> Info:  Answer from slave:          " & integer'image(to_integer(unsigned(dat_from_slave)));
		  report "-> Error: Expected answer from slave: " & integer'image(to_integer(unsigned(compare_value)));
		end if;
	end procedure wb_expect;
	
	component enc_err_counter
		port (
			clk_sys_i     : in std_logic;
			clk_ref_i	  : in std_logic;
			rstn_sys_i    : in std_logic;

			slave_o       : out t_wishbone_slave_out;
			slave_i       : in  t_wishbone_slave_in;

			enc_err_i	  : in std_logic
		);
	end component;
begin------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	dut : enc_err_counter
		port map (
			clk_sys_i     => s_clk_sys,
			clk_ref_i	  => s_clk_ref,
			rstn_sys_i    => s_rst_n,

			slave_o       => s_wb_slave_out,
			slave_i       => s_wb_slave_in,

			enc_err_i	  => enc_err
		);

	-- Reset controller
	p_reset : process
	begin
		wait for c_reset_time;
		s_rst_n <= '1';
	end process;
	s_rst <= not(s_rst_n);
	
	-- Short ack
 	s_ack <= s_wb_slave_out.ack;
 	
 	-- Testbench controller
	p_tb_ctl : process(s_clk_sys, s_rst_n) is
	begin
		if s_rst_n = '0' then
			s_sequence_cnt <= (others => '0');
		elsif rising_edge(s_clk_sys) then
			s_sequence_cnt <= std_logic_vector(unsigned(s_sequence_cnt) + 1);
		end if;
	end process;
 	
 	-- Wishbone controller
	p_wishbone_stim : process
	begin
		-- Reset
		s_wb_slave_in <= wb_stim(c_cyc_off, c_str_off, c_we_off, c_reg_all_zero, c_reg_all_zero);
		wait until rising_edge(s_rst_n);
		wait until rising_edge(s_clk_sys);
		wait for c_sys_clock_cycle*100;
		
		--check error counter
		
		
		--Finish simulation; copied from i2c_testbench.vhd
		wait for c_sys_clock_cycle*10000;
		-- Using STOP_TIME from settings.sh here
		--report "Simulation done!" severity failure;
	end process;
