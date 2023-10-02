--extended version of the quench_detection.vhd file for QuD_Trigger Matrices with 6 IN/OUT Cards
-- author: A. Russo

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;



entity TM_quench_detection is
    Port ( clk : in STD_LOGIC;
           nReset : in STD_LOGIC;
           time_pulse : in STD_LOGIC;
           delay: in STD_LOGIC_VECTOR(23 DOWNTO 0);
           quench_in_out_sel : in STD_LOGIC_VECTOR(11 downto 0);
           QuDIn: in STD_LOGIC_VECTOR (53 downto 0);
           quench_en: in STD_LOGIC_VECTOR (53 downto 0);
           mute: in STD_LOGIC_VECTOR (53 downto 0);  
           QuDOut : out STD_LOGIC_VECTOR(23 downto 0));
end TM_quench_detection;

architecture Arch_quench_detection of TM_quench_detection is

    component e_quench_detection 
        Port ( clk : in STD_LOGIC;
               nReset : in STD_LOGIC;
               time_pulse : in STD_LOGIC;
               delay: in STD_LOGIC;
            
               QuDIn: in STD_LOGIC_VECTOR (53 downto 0);
               mute: in STD_LOGIC_VECTOR (53 downto 0);
               QuDOut : out STD_LOGIC);

    end component; 

Type delay_count_type is array (1 to 23) of std_logic_vector (11 downto 0);
--signal delay_count  :    std_logic_vector (11 downto 0):=X"032";
signal delay_count  : delay_count_type;

signal delay_ctr_tc :    std_logic_vector(23 downto 0);

signal QuD_data_out: STD_LOGIC_VECTOR (23 downto 0):=(others =>'0');
signal delay_ctr_load :  std_logic_vector(23 downto 0);
signal combi : std_logic_vector(23 downto 0):=(others =>'0');
signal int_sel: integer range 0 to 63:=0;
signal out_sel: integer range 0 to 23:=0;

begin

    TM_quench_det_0_out12_1: e_quench_detection 
    Port map( clk => clk,
           nReset => nReset,
           time_pulse => time_pulse,
           delay=> delay(0),
        
           QuDIn=> QuDIN,
           mute=>mute,
           QuDOut => QuD_data_Out(0));

    TM_quench_det_gen_1_23: for i in 1 to 23 generate 
     qud_elem: e_quench_detection 
     Port map( clk => clk,
            nReset => nReset,
            time_pulse => time_pulse,
            delay=> delay(i),
         
            QuDIn=> QuDIN,
            mute=>mute  or not (quench_en ) ,
            QuDOut => QuD_data_Out(i));
end generate TM_quench_det_gen_1_23;

data_process: process (clk, nReset)
begin
    if nReset = '0' then

        int_sel <= 0;
        combi <= (others =>'0');

    elsif rising_edge (clk) then
        
        
        int_sel <= to_integer(unsigned(quench_in_out_sel(5 downto 0)));
        out_sel <= to_integer(unsigned(quench_in_out_sel(11 downto 6)));
        if int_sel = 0 then 
        combi(0) <= QuD_data_Out(0);
        else
        --for int_sel in 1 to 23 loop
            combi(out_sel) <= QUD_data_out(int_sel);
       -- end loop;
        end if;
    end if;
end process;
       

QuDout <= combi;
end Arch_quench_detection;