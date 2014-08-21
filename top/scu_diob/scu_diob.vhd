library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.gencores_pkg.all;
use work.scu_bus_slave_pkg.all;
use work.aux_functions_pkg.all;
use work.fg_quad_pkg.all;
use work.diob_sys_clk_local_clk_switch_pkg.all;




----------------------------------------------------------------------------------------------------------------------
--  Vers: 1 Revi: 0: erstellt am 28.01.2014, Autor: R.Hartmann                                                      --
--                                                                                                                  --
--                                                                                                                  --
--    Config-Register-Layout                                                                                        --
--                                                                                                                  --
--      Base_addr   : Config-Register-Layout                                                                        --
--                                ------+-----------------------------------------------------------------------    --
--                     Lesen Bit:   15  | Leiterplatten-Test-Mode;  1 = Test                                        --
--                                      |                           0 = Betrieb                                     --
--                                ------+-----------------------------------------------------------------------    --
--                                      |                                                                           --
--                                14..3 | immer '0'                                                                 --
--                                      |                                                                           --
--                                ------+-----------------------------------------------------------------------    --
--                                  2   | ADC_mode;  1 = ADC-Daten aus dem Speicher, gespeichert mit EOC				  --
--                                      |            0 = ADC-Daten die am Sub-D Stecker anstehen.						  --
--                                ------+-----------------------------------------------------------------------    --
--                                  1   | FG_mode;  1 = Funktiongenerator-Mode, DAC-Werte kommen von FG_Data und    --
--                                      |               werden mit FG_Strobe uebernommen. Kein externer Trigger!    --
--                                      |           0 = Software-Mode, DAC-Werte, kommen vom SCU-Bus-Slave.         --
--                                ------+-----------------------------------------------------------------------    --
--                                  0   |  --
--                                ------+-----------------------------------------------------------------------    --
--                                                                                                                  --
--                  
--                                ------+-----------------------------------------------------------------------    --
--                 Schreiben Bit:   15  | Leiterplatten-Test-Mode;  1 = Test                                        --
--                                      |                           0 = Betrieb                                     --
--                                ------+-----------------------------------------------------------------------    --
--                                      |                                                                           --
--                                14..3 | kein Einfluss                                                             --
--                                      |                                                                           --
--                                ------+-----------------------------------------------------------------------    --
--                                  2   | ADC_mode;  1 = ADC-Daten aus dem Speicher, gespeichert mit EOC				  --
--                                      |            0 = ADC-Daten die am Sub-D Stecker anstehen.						  --
--                                ------+-----------------------------------------------------------------------    --
--                                  1   | FG_mode;  1 = Funktiongenerator-Mode, DAC-Werte kommen von FG_Data und    --
--                                      |               werden mit FG_Strobe uebernommen. Kein externer Trigger!    --
--                                      |           0 = Software-Mode, DAC-Werte, kommen vom SCU-Bus-Slave.         --
--                                ------+-----------------------------------------------------------------------    --
--                                  0   |  --
--                                ------+-----------------------------------------------------------------------    --
--                                                                                                                  --
----------------------------------------------------------------------------------------------------------------------


entity scu_diob is
generic (
    CLK_sys_in_Hz:      integer := 125000000
    );
port  (
    ------------------------------ Clocks -------------------------------------------------------------------------
    CLK_20MHz_A: in std_logic; -- Clock_A
    CLK_20MHz_B: in std_logic; -- Clock_B
    CLK_20MHz_C: in std_logic; -- Clock_C
    CLK_20MHz_D: in std_logic; -- Clock_D
   
    --------- Parallel SCU-Bus-Signale ----------------------------------------------------------------------------
    A_A: in std_logic_vector(15 downto 0); -- SCU-Adressbus
    A_nADR_EN: out std_logic := '0'; -- '0' => externe Adresstreiber des Slaves aktiv
    A_nADR_FROM_SCUB: out std_logic := '0'; -- '0' => externe Adresstreiber-Richtung: SCU-Bus nach Slave
    A_D: inout std_logic_vector(15 downto 0); -- SCU-Datenbus
    A_nDS: in std_logic; -- Data-Strobe vom Master gertieben
    A_RnW: in std_logic; -- Schreib/Lese-Signal vom Master getrieben, '0' => lesen
    A_nSel_Ext_Data_Drv: out std_logic; -- '0' => externe Datentreiber des Slaves aktiv
    A_Ext_Data_RD: out std_logic; -- '0' => externe Datentreiber-Richtung: SCU-Bus nach
                                                                -- Slave (besser default 0, oder Treiber A/B tauschen)
                                                                -- SCU-Bus nach Slave (besser default 0, oder Treiber A/B tauschen)
    A_nDtack: out std_logic; -- Data-Acknowlege null aktiv, '0' => aktiviert externen
                                                                -- Opendrain-Treiber
    A_nSRQ: out std_logic; -- Service-Request null aktiv, '0' => aktiviert externen
                                                                -- Opendrain-Treiber
    A_nBoardSel: in std_logic; -- '0' => Master aktiviert diesen Slave
    A_nEvent_Str: in std_logic; -- '0' => Master sigalisiert Timing-Zyklus
    A_SysClock: in std_logic; -- Clock vom Master getrieben.
    A_Spare0: in std_logic; -- vom Master getrieben
    A_Spare1: in std_logic; -- vom Master getrieben
    A_nReset: in std_logic; -- Reset (aktiv '0'), vom Master getrieben

    A_nSEL_Ext_Signal_DRV: out std_logic; -- '0' => Treiber fr SCU-Bus-Steuer-Signale aktiv
    A_nExt_Signal_in: out std_logic; -- '0' => Treiber fr SCU-Bus-Steuer-Signale-Richtung: SCU-Bus nach Slave (besser default 0, oder Treiber A/B tauschen)

    ----------------- OneWire ----------------------------------------------------------------------------------------
    A_OneWire: inout std_logic; -- Temp.-OneWire auf dem Slave
    
    ------------ Logic analyser Signals -------------------------------------------------------------------------------
    A_SEL: in std_logic_vector(3 downto 0); -- use to select sources for the logic analyser ports
    A_Tclk: out std_logic; -- Clock  for Logikanalysator Port A
    A_TA: inout std_logic_vector(15 downto 0); -- test port a

    ---------------------------------- Diagnose-LED's -----------------------------------------------------------------
    A_nLED_D2: out std_logic; -- Diagnose-LED_D2 auf dem Basis-Board
    A_nLED_D3: out std_logic; -- Diagnose-LED_D3 auf dem Basis-Board

    ------------ User I/O zur VG-Leiste -------------------------------------------------------------------------------
    A_nUser_EN: out std_logic; -- Enable User-I/O
    UIO: inout std_logic_vector(15 downto 0); -- User I/O VG-Leiste
  
    ---------------- bergabestecker fr Anwender-I/O -----------------------------------------------------------------
    CLK_IO: in std_logic; -- Clock vom Anwender_I/0
    PIO: inout std_logic_vector(150 downto 16)  -- Dig. User I/0 to Piggy
    );
end scu_diob;



architecture scu_diob_arch of scu_diob is



--  +============================================================================================================================+
--  |                                                Anfang: Component                                                           |
--  +============================================================================================================================+


component aw_io_reg
generic (
    AW_Base_addr  : integer;
    Tag_Base_addr : integer
    );
port (Adr_from_SCUB_LA: in std_logic_vector(15 downto 0);
    Data_from_SCUB_LA:  in std_logic_vector(15 downto 0);
    Ext_Adr_Val:        in std_logic;
    Ext_Rd_active:      in std_logic;
    Ext_Rd_fin:         in std_logic;
    Ext_Wr_active:      in std_logic;
    Ext_Wr_fin:         in std_logic;
    Timing_Pattern_LA:  in std_logic_vector(31 downto 0);-- latched timing pattern from SCU_Bus for external user functions
    Timing_Pattern_RCV: in std_logic;							-- timing pattern received
    clk:                in std_logic;
    nReset:             in std_logic;
    AWin1:              in std_logic_vector(15 downto 0);
    AWin2:              in std_logic_vector(15 downto 0);
    AWin3:              in std_logic_vector(15 downto 0);
    AWin4:              in std_logic_vector(15 downto 0);
    AWin5:              in std_logic_vector(15 downto 0);
    AWin6:              in std_logic_vector(15 downto 0);
    AWin7:              in std_logic_vector(15 downto 0);
    AW_Config:          out std_logic_vector(15 downto 0);
    AWOut_Reg1:         out std_logic_vector(15 downto 0);
    AWOut_Reg2:         out std_logic_vector(15 downto 0);
    AWOut_Reg3:         out std_logic_vector(15 downto 0);
    AWOut_Reg4:         out std_logic_vector(15 downto 0);
    AWOut_Reg5:         out std_logic_vector(15 downto 0);
    AWOut_Reg6:         out std_logic_vector(15 downto 0);
    AWOut_Reg7:         out std_logic_vector(15 downto 0);
    AW_Config_Wr:       out std_logic;  
    AWOut_Reg1_Wr:      out std_logic;   
    AWOut_Reg2_Wr:      out std_logic;   
    AWOut_Reg3_Wr:      out std_logic;   
    AWOut_Reg4_Wr:      out std_logic;   
    AWOut_Reg5_Wr:      out std_logic;   
    AWOut_Reg6_Wr:      out std_logic;   
    AWOut_Reg7_Wr:      out std_logic;   
    AWOut_Reg_rd_active:out std_logic;
    Data_to_SCUB:       out std_logic_vector(15 downto 0);
    Dtack_to_SCUB:      out std_logic;
    LA_aw_io_reg:       out std_logic_vector(15 downto 0)
	 );
end component aw_io_reg;


COMPONENT io_reg
	GENERIC ( Base_addr : INTEGER );
	PORT
	(
		Adr_from_SCUB_LA:		IN STD_LOGIC_VECTOR(15 DOWNTO 0);
		Data_from_SCUB_LA:	IN STD_LOGIC_VECTOR(15 DOWNTO 0);
		Ext_Adr_Val:	 		IN STD_LOGIC;
		Ext_Rd_active:			IN STD_LOGIC;
		Ext_Rd_fin:			 	IN STD_LOGIC;
		Ext_Wr_active:			IN STD_LOGIC;
		Ext_Wr_fin:				IN STD_LOGIC;
		clk:	 					IN STD_LOGIC;
		nReset:	 				IN STD_LOGIC;
		Reg_IO1:					OUT STD_LOGIC_VECTOR(15 DOWNTO 0);
		Reg_IO2:					OUT STD_LOGIC_VECTOR(15 DOWNTO 0);
		Reg_IO3:					OUT STD_LOGIC_VECTOR(15 DOWNTO 0);
		Reg_IO4:					OUT STD_LOGIC_VECTOR(15 DOWNTO 0);
		Reg_IO5:					OUT STD_LOGIC_VECTOR(15 DOWNTO 0);
		Reg_IO6:					OUT STD_LOGIC_VECTOR(15 DOWNTO 0);
		Reg_IO7:					OUT STD_LOGIC_VECTOR(15 DOWNTO 0);
		Reg_IO8:					OUT STD_LOGIC_VECTOR(15 DOWNTO 0);
		Reg_rd_active:	 		OUT STD_LOGIC;
		Data_to_SCUB:	 		OUT STD_LOGIC_VECTOR(15 DOWNTO 0);
		Dtack_to_SCUB:	 		OUT STD_LOGIC
	);
END COMPONENT io_reg;



component flash_loader_v01
port  (
    noe_in:   in    std_logic
    );
end component;


component pu_reset
generic (
    PU_Reset_in_clks : integer
    );
port  (
    Clk:    in    std_logic;
    PU_Res:   out   std_logic
    );
end component;



component zeitbasis
generic (
    CLK_in_Hz:      integer;
    diag_on:      integer
    );
port  (
    Res:        in  std_logic;
    Clk:        in  std_logic;
    Ena_every_100ns:  out std_logic;
    Ena_every_166ns:  out std_logic;
    Ena_every_250ns:  out std_logic;
    Ena_every_500ns:  out std_logic;
    Ena_every_1us:    out std_logic;
    Ena_Every_20ms:   out std_logic
    );
end component;


--  +============================================================================================================================+
--  |                                                    Ende: Component                                                         |
--  +============================================================================================================================+

  signal clk_sys, clk_cal, locked : std_logic;

  signal SCUB_SRQ: std_logic;
  signal SCUB_Dtack: std_logic;
  signal convst: std_logic;
  signal rst: std_logic;
  
  signal Dtack_to_SCUB: std_logic;
  
  signal ADR_from_SCUB_LA: std_logic_vector(15 downto 0);
  signal Data_from_SCUB_LA: std_logic_vector(15 downto 0);
  signal Ext_Adr_Val: std_logic;
  signal Ext_Rd_active: std_logic;
  signal Ext_Wr_active: std_logic;
  signal Ext_Wr_fin_ovl: std_logic;
  signal SCU_Ext_Wr_fin: std_logic;
  signal nPowerup_Res: std_logic;
  signal Timing_Pattern_LA:  std_logic_vector(31 downto 0);--  latched timing pattern from SCU_Bus for external user functions
  signal Timing_Pattern_RCV: std_logic;-------------------  timing pattern received
  
  signal extension_cid_system:  integer range 0 to 16#FFFF#;  -- in,  extension card: cid_system
  signal extension_cid_group:   integer range 0 to 16#FFFF#;  --in, extension card: cid_group
 
  signal fg_1_dtack: std_logic;
  signal fg_1_data_to_SCUB: std_logic_vector(15 downto 0);
  signal fg_1_rd_active: std_logic;
  signal fg_1_sw: std_logic_vector(31 downto 0);
  signal fg_1_strobe: std_logic;
  signal fg_1_dreq: std_logic;
  
  signal led_ena_cnt: std_logic;

  signal Data_to_SCUB: std_logic_vector(15 downto 0);
  
  signal reset_clks : std_logic_vector(0 downto 0);
  signal reset_rstn : std_logic_vector(0 downto 0);
  signal clk_sys_rstn : std_logic;
  
  signal owr_pwren_o: std_logic_vector(1 downto 0);
  signal owr_en_o: std_logic_vector(1 downto 0);
  signal owr_i:        std_logic_vector(1 downto 0);
  
  signal wb_scu_rd_active: std_logic;
  signal wb_scu_dtack: std_logic;
  signal wb_scu_data_to_SCUB: std_logic_vector(15 downto 0);
  
  signal irqcnt:  unsigned(12 downto 0);
  

  signal Powerup_Res: std_logic; -- only for modelsim!
  signal Powerup_Done: std_logic;     -- this memory is set to one if an Powerup is done. Only the SCUB-Master can clear this bit.
  signal WRnRD: std_logic; -- only for modelsim!

  signal clk, la_clk: std_logic;
  signal Deb_SCUB_Reset_out: std_logic;
  signal Standard_Reg_Acc:    std_logic;
  signal Ext_Rd_fin: std_logic;
  signal Ext_Wr_fin: std_logic;

  
  signal Ena_Every_100ns: std_logic;
  signal Ena_Every_166ns: std_logic;
  signal Ena_Every_20ms: std_logic;
  signal Ena_Every_1us: std_logic;
  signal Ena_Every_250ms: std_logic;
  signal Ena_Every_500ms: std_logic;
 
  signal F_12p5_MHz: std_logic;
   
  signal test_port_in_0: std_logic_vector(15 downto 0);
  signal test_clocks: std_logic_vector(15 downto 0);
  
  signal s_nLED_Sel: std_logic; -- LED = Sel
  signal s_nLED_Dtack: std_logic; -- LED = Dtack
  signal s_nLED_inR: std_logic; -- LED = interrupt
  signal s_nLED_PU: std_logic; -- LED = Powerup_Reset
   
  signal s_nLED: std_logic_vector(7 downto 0); -- LED's
  signal s_nLED_Out: std_logic_vector(7 downto 0); -- LED's
  signal s_AW_ID: std_logic_vector(7 downto 0); -- Anwender_ID
   
  signal AWin1: std_logic_vector(15 downto 0);
  signal AWin2: std_logic_vector(15 downto 0);
  signal AWin3: std_logic_vector(15 downto 0);
  signal AWin4: std_logic_vector(15 downto 0);
  signal AWin5: std_logic_vector(15 downto 0);
  signal AWin6: std_logic_vector(15 downto 0);
  signal AWin7: std_logic_vector(15 downto 0);
  signal AW_Config: std_logic_vector(15 downto 0);
  signal AWOut_Reg1: std_logic_vector(15 downto 0);
  signal AWOut_Reg2: std_logic_vector(15 downto 0);
  signal AWOut_Reg3: std_logic_vector(15 downto 0);
  signal AWOut_Reg4: std_logic_vector(15 downto 0);
  signal AWOut_Reg5: std_logic_vector(15 downto 0);
  signal AWOut_Reg6: std_logic_vector(15 downto 0);
  signal AWOut_Reg7: std_logic_vector(15 downto 0);
  signal AW_Config_Wr:  std_logic;  
  signal AWOut_Reg1_Wr: std_logic;
  signal AWOut_Reg2_Wr: std_logic;
  signal AWOut_Reg3_Wr: std_logic;
  signal AWOut_Reg4_Wr: std_logic;
  signal AWOut_Reg5_Wr: std_logic;
  signal AWOut_Reg6_Wr: std_logic;
  signal AWOut_Reg7_Wr: std_logic;
  signal AWOut_Reg_rd_active: std_logic;
  signal aw_port1_Dtack: std_logic;
  signal aw_port1_data_to_SCUB: std_logic_vector(15 downto 0);
  signal LA_aw_io_reg: std_logic_vector(15 downto 0);



  signal INL_msk_IO1: std_logic_vector(15 downto 0);	       
  signal INL_msk_IO2: std_logic_vector(15 downto 0);	       
  signal INL_msk_IO3: std_logic_vector(15 downto 0);			 
  signal INL_msk_IO4: std_logic_vector(15 downto 0);	       
  signal INL_msk_IO5: std_logic_vector(15 downto 0);	       
  signal INL_msk1_rd_active: std_logic;	 
  signal INL_msk1_Dtack: std_logic;      
  signal INL_msk1_data_to_SCUB: std_logic_vector(15 downto 0);


  signal INL_xor_IO1: std_logic_vector(15 downto 0);		    
  signal INL_xor_IO2: std_logic_vector(15 downto 0);		    
  signal INL_xor_IO3: std_logic_vector(15 downto 0);		    
  signal INL_xor_IO4: std_logic_vector(15 downto 0);		    
  signal INL_xor_IO5: std_logic_vector(15 downto 0);		    
  signal INL_xor1_rd_active: std_logic;	 
  signal INL_xor1_Dtack: std_logic;      
  signal INL_xor1_data_to_SCUB: std_logic_vector(15 downto 0);
       

  signal INL_IO1: std_logic_vector(15 downto 0);		    
  signal INL_IO2: std_logic_vector(15 downto 0);		    
  signal INL_IO3: std_logic_vector(15 downto 0);		    
  signal INL_IO4: std_logic_vector(15 downto 0);		    
  signal INL_IO5: std_logic_vector(15 downto 0);		    
  signal Interlock: std_logic;		    


	
  signal hp_la_o:			std_logic_vector(15 downto 0); -- Output für HP-Logicanalysator
  
   
  signal IO_Test_Port0: std_logic_vector(15 downto 0);
  signal IO_Test_Port1: std_logic_vector(15 downto 0);
  signal IO_Test_Port2: std_logic_vector(15 downto 0);
  signal IO_Test_Port3: std_logic_vector(15 downto 0);
  
  signal s_nLED_User1_i: std_logic;	-- LED3 = User 1 
  signal s_nLED_User2_i: std_logic;	-- LED2 = User 2 
  signal s_nLED_User3_i: std_logic;	-- LED1 = User 3 
  signal s_nLED_User1_o: std_logic;	-- LED3 = User 1 
  signal s_nLED_User2_o: std_logic;	-- LED2 = User 2 
  signal s_nLED_User3_o: std_logic;	-- LED1 = User 3 

  signal uart_txd_out:  std_logic;


  
--  +============================================================================================================================+
--  |                                   Übergabe-Signale für Anwender-IO: P37IO  -- FG900_700                                    |
--  +============================================================================================================================+

  signal P37IO_Start_i:			std_logic;		-- input "Start" L-Aktiv
  signal P37IO_Start_deb_o:	std_logic;		-- input "Start" entprellt
  signal P37IO_nLED_Start_o:	std_logic;		--	Output "nLED_Start"
  signal P37IO_Stop_i:			std_logic;     -- input "Stop" L-Aktiv
  signal P37IO_Stop_deb_o:		std_logic;     -- input "Stop" entprellt
  signal P37IO_nLED_Stop_o:	std_logic;		--	Output "nLED_Stop"
  signal P37IO_Reset_i:			std_logic;     -- input "Reset" L-Aktiv
  signal P37IO_Reset_deb_o:	std_logic;     -- input "Rest" entprellt
  signal P37IO_BNC_o:			std_logic;   	-- Output "BNC"
  signal P37IO_nELD_BNC_o:		std_logic;     -- Output "nLED_BNC"

  signal P37IO_in_Data_Deb_i: std_logic_vector(15 downto 0); -- Data_Input über Optokoppler
  signal P37IO_in_Data_Deb_o: std_logic_vector(15 downto 0); -- entprellter Data_Input

  
--  +============================================================================================================================+
--  |                                   Übergabe-Signale für Anwender-IO: P25IO  -- FG900_710                                    |
--  +============================================================================================================================+

	signal P25IO_Start_i:				std_logic;				-- input "Start" L-Aktiv
	signal P25IO_Start_deb_o:			std_logic;				-- input "Start" entprellt
	signal P25IO_nLED_Start_o:			std_logic;				--	Output "nLED_Start"
	signal P25IO_Stop_i:					std_logic;     		-- input "Stop" L-Aktiv
	signal P25IO_Stop_deb_o:			std_logic;     		-- input "Stop" entprellt
	signal P25IO_nLED_Stop_o:			std_logic;				--	Output "nLED_Stop"
	signal P25IO_Reset_i:				std_logic;     		-- input "Reset" L-Aktiv
	signal P25IO_Reset_deb_o:			std_logic;     		-- input "Rest" entprellt
	signal P25IO_BNC_o:					std_logic;   			-- Output "BNC"
	signal P25IO_nELD_BNC_o:			std_logic;     		-- Output "nLED_BNC"

   signal P25IO_DAC_Out: 		      std_logic_vector(15 downto 0); -- Zwischenspeicher
	signal P25IO_DAC_Strobe_Start_i:	std_logic;				-- input  "Start-Signal für den Stobe vom DAC"
	signal P25IO_DAC_Strobe_Start_o:	std_logic;				-- Output "Start-Puls für den Stobe vom DAC (1 CLK breit)"
	signal P25IO_DAC_Strobe_i:			std_logic;				-- input  "Start-Puls für den DAC-Strobe"
	signal P25IO_nDAC_Strobe_o:		std_logic;				-- Output "DAC-Stobe"
	signal P25IO_DAC_shift: std_logic_vector(2  downto 0); -- Shift-Reg.
 
	signal P25IO_ADC_ECC_Start_i:	std_logic;					-- input  "Start-Signal für das Enable vom ADC"
	signal P25IO_ADC_ECC_Start_o:	std_logic;					-- Output "Start-Puls für das Enable vom ADC (1 CLK breit)"
	signal P25IO_ADC_ECC_i:			std_logic;					-- input  "Start-Puls für das Enable vom ADC"
	signal P25IO_nADC_ECC_o:		std_logic;					-- Output "ADC-Enable"
	signal P25IO_ADC_shift: std_logic_vector(2  downto 0); -- Shift-Reg.
 
	signal P25IO_Ext_Tim_i:			std_logic;					-- input "Start" L-Aktiv
	signal P25IO_Ext_Tim_deb_o:	std_logic;					-- input "Start" entprellt
	signal P25IO_nLED_Ext_Tim_o:	std_logic;					--	Output "nLED_Start"


	signal P25IO_Ext_Tim_Strobe_Start_o:	std_logic;				-- Output "Start-Puls für den ext. Trigger"
	signal P25IO_Ext_Tim_shift: std_logic_vector(2  downto 0);	-- Shift-Reg.

	
	signal P25IO_EOC_i:		 		std_logic;								--	input "nEOC"
	signal P25IO_EOC_deb_o:			std_logic;  							-- input "EOC" entprellt
	signal P25IO_ADC_Data_FF_i:	std_logic_vector(15  downto 0);	-- input  "Daten ADC-Register"
	signal P25IO_ADC_Data_FF_o:	std_logic_vector(15  downto 0);  -- Output "Daten ADC-Register"
 
 
 
  signal s_str_shift_EE_20ms: std_logic_vector(2  downto 0);  -- Shift-Reg.
  signal s_str_EE_20ms: std_logic;                -- Puls-Output


  signal P25IO_in_Data_Deb_i: std_logic_vector(15 downto 0); -- Data_Input über Optokoppler
  signal P25IO_in_Data_Deb_o: std_logic_vector(15 downto 0); -- entprellter Data_Input

  
--  +============================================================================================================================+
--  |                                   Übergabe-Signale für Anwender-IO: OCIN   -- FG900_720                                    |
--  +============================================================================================================================+


  signal OCIN_in_Data_Deb1_i: std_logic_vector(15 downto 0); -- Data_Input über Optokoppler
  signal OCIN_in_Data_Deb1_o: std_logic_vector(15 downto 0); -- entprellter Data_Input
  signal OCIN_in_Data_Deb2_i: std_logic_vector(15 downto 0); -- Data_Input über Optokoppler
  signal OCIN_in_Data_Deb2_o: std_logic_vector(15 downto 0); -- entprellter Data_Input


--  +============================================================================================================================+
--  |                                   Übergabe-Signale für Anwender-IO: OCIO   -- FG900_730                                    |
--  +============================================================================================================================+


  signal OCIO_in_Data_Deb1_i: std_logic_vector(15 downto 0); -- Data_Input über Optokoppler
  signal OCIO_in_Data_Deb1_o: std_logic_vector(15 downto 0); -- entprellter Data_Input
  signal OCIO_in_Data_Deb2_i: std_logic_vector(7  downto 0); -- Data_Input über Optokoppler
  signal OCIO_in_Data_Deb2_o: std_logic_vector(7  downto 0); -- entprellter Data_Input


--  +============================================================================================================================+
--  |                                   Übergabe-Signale für Anwender-IO: SPSIO  -- FG900.770                                    |
--  +============================================================================================================================+

  signal SPSIO_in_Data_Deb_i: std_logic_vector(23 downto 0); -- Data_Input über Optokoppler
  signal SPSIO_in_Data_Deb_o: std_logic_vector(23 downto 0); -- entprellter Data_Input


--  +============================================================================================================================+
--  |                                   Übergabe-Signale für Anwender-IO: HFIO  -- FG900.780                                     |
--  +============================================================================================================================+

	signal HFIO_Tastpuls_i: 					std_logic;	--	Input  "nLED_Tastpuls"
	signal HFIO_nLED_Tastpuls_o: 				std_logic;	--	Output "nLED_Tastpuls"
	signal HFIO_Sample_Puls_inv_i:			std_logic;	--	Input  "nLED_Sample_Puls_inv"
	signal HFIO_nLED_Sample_Puls_inv_o: 	std_logic;	--	Output "nLED_Sample_Puls_inv"
	signal HFIO_Aux_i: 							std_logic;	--	Input  "nLED_Aux"
	signal HFIO_nLED_Aux_o: 					std_logic;	--	Output "nLED_Aux"
	signal HFIO_Sample_Puls_Display_i:		std_logic;	--	Input  "nLED_Sample_Puls_Display"
	signal HFIO_nLED_Sample_Puls_Display_o:std_logic;	--	Output "nLED_Sample_Puls_Display"

	signal HFIO_in_AMP_FEHLER_Deb_i:			std_logic;	-- Input  "AMP_FEHLER"
	signal HFIO_in_AMP_FEHLER_Deb_o:			std_logic;	-- Output "AMP_FEHLER"
	signal HFIO_in_PHASE_FEHLER_Deb_i:		std_logic;	-- Input  "PHASE_FEHLER"
	signal HFIO_in_PHASE_FEHLER_Deb_o:		std_logic;	-- Output  PHASE_FEHLER"
	
	

--  +============================================================================================================================+

--  signal Deb_in:    	std_logic_vector(15 downto 0);
--  signal Deb_out:    std_logic_vector(15 downto 0);

--  +============================================================================================================================+


  signal clk_blink: std_logic;
  
  signal sys_clk_is_bad:        std_logic;
  signal sys_clk_is_bad_led_n:  std_logic;
  signal sys_clk_is_bad_la:     std_logic;
  signal local_clk_is_bad:      std_logic;
  signal local_clk_is_running:  std_logic;
  signal local_clk_runs_led_n:  std_logic;
  signal sys_clk_failed:        std_logic;
  signal sys_clk_deviation:     std_logic;
  signal sys_clk_deviation_la:  std_logic;
  signal sys_clk_deviation_led_n: std_logic;
  signal clk_switch_rd_data:    std_logic_vector(15 downto 0);
  signal clk_switch_rd_active:  std_logic;
  signal clk_switch_dtack:      std_logic;
  signal pll_locked:            std_logic;
  signal clk_switch_intr:       std_logic;
  
  signal signal_tap_clk_250mhz: std_logic;
  
  
--  constant  C_Debounce_input_in_ns: integer := 5000;

  constant  C_P37IO_Start_Debounce_input_in_ns: 	integer := 5000;		-- Debounce, Start-Input von FG900_700
  constant  C_P37IO_Stop_Debounce_input_in_ns: 		integer := 5000;		-- Debounce, Stop-Input  von FG900_700
  constant  C_P37IO_Reset_Debounce_input_in_ns: 	integer := 5000;		-- Debounce, Reset-Input von FG900_700
  constant  C_P37IO_Data_Debounce_input_in_ns: 		integer := 5000;		-- Debounce, Data-Input  von FG900_700
                                                                     
  constant  C_P25IO_Start_Debounce_input_in_ns: 	integer := 5000;		-- Debounce, Data-Input von FG900_710
  constant  C_P25IO_Stop_Debounce_input_in_ns: 		integer := 5000;		-- Debounce, Data-Input von FG900_710
  constant  C_P25IO_Reset_Debounce_input_in_ns: 	integer := 5000;		-- Debounce, Data-Input von FG900_710
  constant  C_P25IO_Ext_Tim_Debounce_input_in_ns:	integer := 5000;		-- Debounce, Data-Input von FG900_710
  constant  C_P25IO_EOC_Debounce_input_in_ns: 		integer := 500;		-- Debounce, Data-Input von FG900_710
  constant  C_P25IO_Data_Debounce_input_in_ns: 		integer := 5000;		-- Debounce, Data-Input von FG900_710
                                                                     
  constant  C_OCIN_Data_Debounce_input_in_ns: 		integer := 5000;		-- Debounce, Data-Input von FG900_720
  constant  C_OCIO_Data_Debounce_input_in_ns: 		integer := 5000;		-- Debounce, Data-Input von FG900_730

  constant  C_SPSIO_Data_Debounce_input_in_ns: 		integer := 5000;		-- Debounce, Data-Input von FG900_770

  constant  C_HFIO_AMP_FEHLER_Debounce_input_in_ns: 	integer := 5000;	-- Debounce, Data-Input von FG900_780
  constant  C_HFIO_PHASE_FEHLER_Debounce_input_in_ns: integer := 5000;	-- Debounce, Data-Input von FG900_780
 
  
  
  constant  Clk_in_ns: integer:= 1000000000/clk_sys_in_Hz;
  constant  stretch_cnt: integer := 5;
  

  CONSTANT c_AW_P37IO:	std_logic_vector(7 downto 0):= B"00000001"; -- FG900_700
  CONSTANT c_AW_P25IO:	std_logic_vector(7 downto 0):= B"00000010"; -- FG900_710
  CONSTANT c_AW_OCin:	std_logic_vector(7 downto 0):= B"00000011"; -- FG900_720
  CONSTANT c_AW_OCIO:	std_logic_vector(7 downto 0):= B"00000100"; -- FG900_730
  CONSTANT c_AW_UIO:		std_logic_vector(7 downto 0):= B"00000101"; -- FG900_740
  CONSTANT c_AW_DA:		std_logic_vector(7 downto 0):= B"00000110"; -- FG900_750
  CONSTANT c_AW_Frei:	std_logic_vector(7 downto 0):= B"00000111"; -- FG900_760
  CONSTANT c_AW_SPSIO:	std_logic_vector(7 downto 0):= B"00001000"; -- FG900_770
  CONSTANT c_AW_HFIO:	std_logic_vector(7 downto 0):= B"00001001"; -- FG900_780

  
--  +============================================================================================================================+
--  |                                                        Begin                                                               |
--  +============================================================================================================================+


  begin


  A_nADR_EN             <= '0';
  A_nADR_FROM_SCUB      <= '0';
  A_nExt_Signal_in      <= '0';
  A_nSEL_Ext_Signal_DRV <= '0';
  A_nUser_EN            <= '0';
  

--  Deb16:	for I in 0 to 15 generate
--		DB_I:	debounce generic map(DB_Cnt => C_Debounce_input_in_ns / clk_in_ns)
--							port map(DB_in => Deb_in(I),  Reset => Powerup_Res, clk => clk_sys, DB_Out => Deb_out(I));
--		end generate Deb16;
  
  
  Powerup_Res <= not nPowerup_Res;  -- only for modelsim!
  WRnRD       <= not A_RnW;         -- only for modelsim!

  diob_clk_switch: diob_sys_clk_local_clk_switch
    port map(
      local_clk_i           => CLK_20MHz_D,
      sys_clk_i             => A_SysClock,
      nReset                => nPowerup_Res,
      master_clk_o          => clk_sys,               -- core clocking
      pll_locked            => pll_locked,
      sys_clk_is_bad        => sys_clk_is_bad,
      sys_clk_is_bad_la     => sys_clk_is_bad_la,
      local_clk_is_bad      => local_clk_is_bad,
      local_clk_is_running  => local_clk_is_running,
      sys_clk_deviation     => sys_clk_deviation,
      sys_clk_deviation_la  => sys_clk_deviation_la,
      Adr_from_SCUB_LA      => ADR_from_SCUB_LA,      -- in, latched address from SCU_Bus
      Data_from_SCUB_LA     => Data_from_SCUB_LA,     -- in, latched data from SCU_Bus
      Ext_Adr_Val           => Ext_Adr_Val,           -- in, '1' => "ADR_from_SCUB_LA" is valid
      Ext_Rd_active         => Ext_Rd_active,         -- in, '1' => Rd-Cycle is active
      Ext_Wr_active         => Ext_Wr_active,         -- in, '1' => Wr-Cycle is active
      Rd_Port               => clk_switch_rd_data,    -- output for all read sources of this macro
      Rd_Activ              => clk_switch_rd_active,  -- this acro has read data available at the Rd_Port.
      Dtack                 => clk_switch_dtack,
      signal_tap_clk_250mhz    => signal_tap_clk_250mhz
      );
    


      
aw_port1: aw_io_reg     
generic map(
      AW_Base_addr =>   16#200#,
      Tag_Base_addr =>  16#280#
      )
port map  (     
      Adr_from_SCUB_LA    =>  ADR_from_SCUB_LA,
      Data_from_SCUB_LA   =>  Data_from_SCUB_LA,
      Ext_Adr_Val         =>  Ext_Adr_Val,
      Ext_Rd_active       =>  Ext_Rd_active,
      Ext_Rd_fin          =>  Ext_Rd_fin,
      Ext_Wr_active       =>  Ext_Wr_active,
      Ext_Wr_fin          =>  Ext_Wr_fin,
      Timing_Pattern_LA   => Timing_Pattern_LA, -- out,        latched timing pattern from SCU_Bus for external user functions
      Timing_Pattern_RCV  => Timing_Pattern_RCV, -- out,        timing pattern received
      clk                 =>  clk_sys,
      nReset              =>  nPowerup_Res,
      AWin1               =>  AWin1,
      AWin2               =>  AWin2,
      AWin3               =>  AWin3,
      AWin4               =>  AWin4,
      AWin5               =>  AWin5,
      AWin6               =>  AWin6,
      AWin7               =>  AWin7,
      AW_Config           =>  AW_Config,
      AWOut_Reg1          =>  AWOut_Reg1,
      AWOut_Reg2          =>  AWOut_Reg2,
      AWOut_Reg3          =>  AWOut_Reg3,
      AWOut_Reg4          =>  AWOut_Reg4,
      AWOut_Reg5          =>  AWOut_Reg5,
      AWOut_Reg6          =>  AWOut_Reg6,
      AWOut_Reg7          =>  AWOut_Reg7,
      AW_Config_Wr        =>  AW_Config_Wr, 
      AWOut_Reg1_Wr       =>  AWOut_Reg1_Wr,  
      AWOut_Reg2_Wr       =>  AWOut_Reg2_Wr,
      AWOut_Reg3_Wr       =>  AWOut_Reg3_Wr,  
      AWOut_Reg4_Wr       =>  AWOut_Reg4_Wr,  
      AWOut_Reg5_Wr       =>  AWOut_Reg5_Wr,  
      AWOut_Reg6_Wr       =>  AWOut_Reg6_Wr,  
      AWOut_Reg7_Wr       =>  AWOut_Reg7_Wr,  
      AWOut_Reg_rd_active =>  AWOut_Reg_rd_active,
      Dtack_to_SCUB       =>  aw_port1_Dtack,
      Data_to_SCUB        =>  aw_port1_data_to_SCUB,
		LA_aw_io_reg        =>  LA_aw_io_reg
      );


		
inl_xor1: io_reg     
generic map(
      Base_addr =>  16#230#
      )
port map  (     
      Adr_from_SCUB_LA   =>  ADR_from_SCUB_LA,
      Data_from_SCUB_LA  =>  Data_from_SCUB_LA,
      Ext_Adr_Val        =>  Ext_Adr_Val,
      Ext_Rd_active      =>  Ext_Rd_active,
      Ext_Rd_fin         =>  Ext_Rd_fin,
      Ext_Wr_active      =>  Ext_Wr_active,
      Ext_Wr_fin         =>  Ext_Wr_fin,
      clk                =>  clk_sys,
      nReset             =>  nPowerup_Res,
--
		Reg_IO1		       =>  open,		
		Reg_IO2		       =>  INL_xor_IO1,		
		Reg_IO3		       =>  INL_xor_IO2,		
		Reg_IO4		       =>  INL_xor_IO3,		
		Reg_IO5		       =>  INL_xor_IO4,		
		Reg_IO6		       =>  INL_xor_IO5,	
		Reg_IO7			    =>  open,		
		Reg_IO8			    =>  open,		
--
		Reg_rd_active		 =>  INL_xor1_rd_active,
      Dtack_to_SCUB      =>  INL_xor1_Dtack,
      Data_to_SCUB       =>  INL_xor1_data_to_SCUB
		);
		
		
inl_msk1: io_reg     
generic map(
      Base_addr =>  16#240#
      )
port map  (     
      Adr_from_SCUB_LA   =>  ADR_from_SCUB_LA,
      Data_from_SCUB_LA  =>  Data_from_SCUB_LA,
      Ext_Adr_Val        =>  Ext_Adr_Val,
      Ext_Rd_active      =>  Ext_Rd_active,
      Ext_Rd_fin         =>  Ext_Rd_fin,
      Ext_Wr_active      =>  Ext_Wr_active,
      Ext_Wr_fin         =>  Ext_Wr_fin,
      clk                =>  clk_sys,
      nReset             =>  nPowerup_Res,
--
		Reg_IO1		       =>  open,	   
		Reg_IO2		       =>  INL_msk_IO1,   
		Reg_IO3			    =>  INL_msk_IO2,	
		Reg_IO4		       =>  INL_msk_IO3,  
		Reg_IO5		       =>  INL_msk_IO4,  
		Reg_IO6		       =>  INL_msk_IO5,  
		Reg_IO7		       =>  open,	  
		Reg_IO8		       =>  open,	  
--
		Reg_rd_active		 =>  INL_msk1_rd_active,
      Dtack_to_SCUB      =>  INL_msk1_Dtack,
      Data_to_SCUB       =>  INL_msk1_data_to_SCUB
		);

 
  
    timer_irq: process (clk_sys, reset_rstn)
    begin
      if reset_rstn = "0" then
        irqcnt <= '0' & x"FFF";
      elsif rising_edge(clk_sys) then
        if irqcnt(irqcnt'high) = '1' then
          irqcnt <= '0' & x"FFF";
        else
          irqcnt <= irqcnt - 1;
        end if;
      end if;
    end process;



    
testport_mux: process (A_SEL, AW_Config, AWin1, AWOut_Reg1,
                       IO_Test_Port0, IO_Test_Port1, IO_Test_Port2, IO_Test_Port3,
                       test_port_in_0, test_clocks,
							  uart_txd_out)
variable test_out: std_logic_vector(15 downto 0);
begin
  case (not A_SEL) is
    when X"0" => test_out := AW_Config;
    when X"1" => test_out := AWOut_Reg1;
    when X"2" => test_out := AWin1;
    when X"3" => test_out := X"0000";
--
    when X"4" => test_out := IO_Test_Port0;
    when X"5" => test_out := IO_Test_Port1;
    when X"6" => test_out := IO_Test_Port2;
    when X"7" => test_out := X"0000";
--                                                 +--- '1' drives the external max level shifter
    when X"8" => test_out := X"000" & '0' & '0' & '1' & uart_txd_out;
    when X"9" => test_out := X"0000";
    when X"A" => test_out := LA_aw_io_reg;
    when X"B" => test_out := X"0000";
    when X"C" => test_out := Timing_Pattern_RCV & Timing_Pattern_LA(14 downto 0);-- Timing
	 --
    when X"D" => test_out := X"0000";
    when X"E" => test_out := test_clocks;
    when X"F" => test_out := test_port_in_0;
    when others =>  test_out := (others => '0');
  end case;
    hp_la_o <= test_out(15 downto 0);
end process testport_mux;



 

A_Tclk  <=   la_clk;  -- Clock fr Logikanalysator: = Sysclk x 2         

test_port_in_0 <= nPowerup_Res  & clk             & Ena_Every_100ns & Ena_Every_166ns & -- bit15..12
                  Ext_Wr_active & SCU_Ext_Wr_fin  & AWOut_Reg1_wr   & fg_1_strobe     & -- bit11..8
                  la_clk        & pll_locked      & A_RnW & A_nDS   &                   -- bit7..4
                  A_nBoardSel   & fg_1_strobe     & '0'   & SCUB_Dtack                  -- bit3..0
                  ;

            
test_clocks <=  X"0"                                                                              -- bit15..12
              & '0' & signal_tap_clk_250mhz & A_SysClock & CLK_20MHz_D                            -- bit11..8
              & '0' & pll_locked & sys_clk_deviation & sys_clk_deviation_la                       -- bit7..4
              & local_clk_is_running & local_clk_is_bad & sys_clk_is_bad & sys_clk_is_bad_la;     -- bit3..0


IO_Test_Port1 <=  x"00" & s_AW_ID(7 downto 0);-- Anwender_ID
IO_Test_Port2 <=  AWOut_Reg1(15 downto 0);
IO_Test_Port3 <=  AWOut_Reg2(15 downto 0);

fl : flash_loader_v01
port map  (
      noe_in  =>  '0'
      );
  
    
  -- open drain buffer for one wire
        owr_i(0) <= A_OneWire;
        A_OneWire <= owr_pwren_o(0) when (owr_pwren_o(0) = '1' or owr_en_o(0) = '1') else 'Z';
  
zeit1 : zeitbasis
generic map (
      CLK_in_Hz =>  clk_sys_in_Hz,
      diag_on   =>  1
      )
port map  (
      Res         =>  Powerup_Res,
      Clk         =>  clk_sys,
      Ena_every_100ns   =>  Ena_Every_100ns,
      Ena_every_166ns   =>  Ena_Every_166ns,
      Ena_every_250ns   =>  open,
      Ena_every_500ns   =>  open,
      Ena_every_1us     =>  Ena_every_1us,
      Ena_Every_20ms    =>  Ena_Every_20ms
      );

		
p_led_sel: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys, Sig_in => not A_nBoardSel, nLED => s_nLED_Sel);-- LED: sel Board
  
p_led_dtack: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys, Sig_in => SCUB_Dtack, nLED => s_nLED_Dtack);-- LED: Dtack to SCU-Bus

p_led_inr: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys, Sig_in => SCUB_SRQ, nLED => s_nLED_inR);-- LED: interrupt

p_led_pu: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys, Sig_in => not (nPowerup_Res), nLED => s_nLED_PU);-- LED: nPowerup_Reset

  
p_led_user1: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys, Sig_in => s_nLED_User1_i, nLED => s_nLED_User1_o);-- LED3 = User 1

p_led_user2: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys, Sig_in => s_nLED_User2_i, nLED => s_nLED_User2_o);-- LED3 = User 1

p_led_user3: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys, Sig_in => s_nLED_User3_i, nLED => s_nLED_User3_o);-- LED3 = User 1
  
  
  
A_nLED_D2 <=   s_nLED_Sel;    -- Diagnose-LED_D2 = BoardSelekt  
A_nLED_D3 <=   s_nLED_Dtack;  -- Diagnose-LED_D3 = Dtack




sel_every_250ms: div_n
  generic map (n => 12, diag_on => 0)  -- ena nur alle 20ms fr einen Takt aktiv, deshalb 13x20ms = 260ms
    port map  ( res => Powerup_Res,
                clk => clk_sys,
                ena => Ena_Every_20ms,
                div_o => ENA_every_250ms
              );
              
sel_every_500ms: div_n
  generic map (n => 25, diag_on => 0)  -- ena nur alle 20ms fr einen Takt aktiv, deshalb 25x20ms = 500ms
    port map  ( res => Powerup_Res,
                clk => clk_sys,
                ena => Ena_Every_20ms,
                div_o => ENA_every_500ms
              );
				  
              
p_clk_blink:  
process (clk_sys, Powerup_Res, ENA_every_250ms)
begin
  if  ( Powerup_Res    = '1') then
      clk_blink   <= '0';
  elsif (rising_edge(clk_sys)) then
    if (ENA_every_500ms = '1') then
      clk_blink <= not clk_blink;
    end if;
  end if;
end process;
  
            
 

clk_switch_intr <= local_clk_is_running or sys_clk_deviation_la;

SCU_Slave: SCU_Bus_Slave
generic map (
    CLK_in_Hz => clk_sys_in_Hz,
    Firmware_Release        => 0,
    Firmware_Version        => 1,
    CID_System => 55, -- important: 55 => CSCOHW
    CID_Group => 26, --- important: 26 => "FG900500_SCU_Diob1"
    intr_Enable          => b"0000_0000_0000_0001")
port map (
    SCUB_Addr => A_A, -- in,        SCU_Bus: address bus
    nSCUB_Timing_Cyc         => A_nEvent_Str, -- in,        SCU_Bus signal: low active SCU_Bus runs timing cycle
    SCUB_Data => A_D, -- inout,        SCU_Bus: data bus (FPGA tri state buffer)
    nSCUB_Slave_Sel => A_nBoardSel, -- in, SCU_Bus: '0' => SCU master select slave
    nSCUB_DS => A_nDS, -- in,        SCU_Bus: '0' => SCU master activate data strobe
    SCUB_RDnWR => A_RnW, -- in, SCU_Bus: '1' => SCU master read slave
    clk => clk_sys,
    nSCUB_Reset_in => A_nReset, -- in,        SCU_Bus-Signal: '0' => 'nSCUB_Reset_in' is active
    Data_to_SCUB => Data_to_SCUB, -- in,        connect read sources from external user functions
    Dtack_to_SCUB => Dtack_to_SCUB, -- in,        connect Dtack from from external user functions
    intr_in => fg_1_dreq & irqcnt(irqcnt'high) & '0' & '0'  -- bit 15..12
            & x"0"                                          -- bit 11..8
            & x"0"                                          -- bit 7..4
            & '0' & '0' & clk_switch_intr,                  -- bit 3..1
    User_Ready => '1',
    extension_cid_system => extension_cid_system, -- in,  extension card: cid_system
    extension_cid_group => extension_cid_group, --in,     extension card: cid_group
    Data_from_SCUB_LA => Data_from_SCUB_LA, -- out,        latched data from SCU_Bus for external user functions
    ADR_from_SCUB_LA => ADR_from_SCUB_LA, -- out,        latched address from SCU_Bus for external user functions
    Timing_Pattern_LA => Timing_Pattern_LA, -- out,        latched timing pattern from SCU_Bus for external user functions
    Timing_Pattern_RCV => Timing_Pattern_RCV, -- out,        timing pattern received
    nSCUB_Dtack_Opdrn => open, -- out,        for direct connect to SCU_Bus opendrain signal
                                                    -- '0' => slave give dtack to SCU master
    SCUB_Dtack => SCUB_Dtack, -- out,        for connect via ext. open collector driver
                                                    -- '1' => slave give dtack to SCU master
    nSCUB_SRQ_Opdrn => open, -- out,        for direct connect to SCU_Bus opendrain signal
                                                    -- '0' => slave service request to SCU ma
    SCUB_SRQ => SCUB_SRQ, -- out,        for connect via ext. open collector driver
                                                    -- '1' => slave service request to SCU master
    nSel_Ext_Data_Drv => A_nSel_Ext_Data_Drv, -- out,        '0' => select the external data driver on the SCU_Bus slave
    Ext_Data_Drv_Rd         => A_Ext_Data_RD, -- out,        '1' => direction of the external data driver on the
                                                    -- SCU_Bus slave is to the SCU_Bus
    Standard_Reg_Acc => Standard_Reg_Acc, -- out,        '1' => mark the access to register of this macro
    Ext_Adr_Val => Ext_Adr_Val, -- out,        for external user functions: '1' => "ADR_from_SCUB_LA" is valid
    Ext_Rd_active => Ext_Rd_active, -- out,        '1' => Rd-Cycle to external user register is active
    Ext_Rd_fin => open, -- out,        marks end of read cycle, active one for one clock period
                                                    -- of clk past cycle end (no overlap)
    Ext_Rd_Fin_ovl => open, -- out,        marks end of read cycle, active one for one clock period
                                                    -- of clk during cycle end (overlap)
    Ext_Wr_active => Ext_Wr_active, -- out,        '1' => Wr-Cycle to external user register is active
    Ext_Wr_fin => SCU_Ext_Wr_fin, -- out,        marks end of write cycle, active high for one clock period
                                                    -- of clk past cycle end (no overlap)
    Ext_Wr_fin_ovl => Ext_Wr_fin_ovl, -- out, marks end of write cycle, active high for one clock period
                                                    -- of clk before write cycle finished (with overlap)
    Deb_SCUB_Reset_out => Deb_SCUB_Reset_out, -- out,        the debounced 'nSCUB_Reset_in'-signal, is active high,
                                                    -- can be used to reset
                                                    -- external macros, when 'nSCUB_Reset_in' is '0'
    nPowerup_Res => nPowerup_Res, -- out,        this macro generated a power up reset
    Powerup_Done => Powerup_Done          -- out  this memory is set to one if an Powerup is done. Only the SCUB-Master can clear this bit.
    );


lm32_ow: housekeeping
  generic map (
    Base_Addr => x"0040" )
  port map (
    clk_sys => clk_sys,
    n_rst => nPowerup_Res,

    ADR_from_SCUB_LA  => ADR_from_SCUB_LA,
    Data_from_SCUB_LA => Data_from_SCUB_LA,
    Ext_Adr_Val       => Ext_Adr_Val,
    Ext_Rd_active     => Ext_Rd_active,
    Ext_Wr_active     => Ext_Wr_active,
    user_rd_active    => wb_scu_rd_active,
    Data_to_SCUB      => wb_scu_data_to_SCUB,
    Dtack_to_SCUB     => wb_scu_dtack,

    owr_pwren_o       => owr_pwren_o,
    owr_en_o          => owr_en_o,
    owr_i             => owr_i,

    debug_serial_o    => uart_txd_out,
    debug_serial_i    => '0');


    Dtack_to_SCUB <= clk_switch_dtack or wb_scu_dtack or fg_1_dtack or aw_port1_Dtack or INL_msk1_Dtack or INL_xor1_Dtack;


    A_nDtack <= NOT(SCUB_Dtack);
    A_nSRQ   <= NOT(SCUB_SRQ);


    
fg_1: fg_quad_scu_bus
  generic map (
    Base_addr => x"0300",
    clk_in_hz => clk_sys_in_Hz,
    diag_on_is_1 => 0 -- if 1 then diagnosic information is generated during compilation
    )
  port map (

    -- SCUB interface
    Adr_from_SCUB_LA  => ADR_from_SCUB_LA,      -- in, latched address from SCU_Bus
    Data_from_SCUB_LA => Data_from_SCUB_LA,     -- in, latched data from SCU_Bus
    Ext_Adr_Val       => Ext_Adr_Val,           -- in, '1' => "ADR_from_SCUB_LA" is valid
    Ext_Rd_active     => Ext_Rd_active,         -- in, '1' => Rd-Cycle is active
    Ext_Wr_active     => Ext_Wr_active,         -- in, '1' => Wr-Cycle is active
    clk               => clk_sys,               -- in, should be the same clk, used by SCU_Bus_Slave
    nReset            => nPowerup_Res,          -- in, '0' => resets the fg_1
    Rd_Port           => fg_1_data_to_SCUB,     -- out, connect read sources (over multiplexer) to SCUB-Macro
    user_rd_active    => fg_1_rd_active,        -- '1' = read data available at 'Rd_Port'-output
    Dtack             => fg_1_dtack,            -- connect Dtack to SCUB-Macro
    dreq              => fg_1_dreq,             -- request of new parameter set

    -- fg output
    sw_out            => fg_1_sw,               -- 24bit output from fg
    sw_strobe         => fg_1_strobe            -- signals new output data
  );



rd_port_mux:  process ( clk_switch_rd_active, clk_switch_rd_data,
                        wb_scu_rd_active,     wb_scu_data_to_SCUB,
                        fg_1_rd_active,       fg_1_data_to_SCUB,
                        AWOut_Reg_rd_active,  aw_port1_data_to_SCUB,
								INL_msk1_rd_active,	 INL_msk1_data_to_SCUB,
								INL_xor1_rd_active,	 INL_xor1_data_to_SCUB )

  variable sel: unsigned(5 downto 0);
  begin
    sel :=  clk_switch_rd_active & wb_scu_rd_active & fg_1_rd_active & AWOut_Reg_rd_active & INL_msk1_rd_active & INL_xor1_rd_active;
    case sel IS
      when "000001" => Data_to_SCUB <= INL_xor1_data_to_SCUB;
      when "000010" => Data_to_SCUB <= INL_msk1_data_to_SCUB;
      when "000100" => Data_to_SCUB <= aw_port1_data_to_SCUB;
      when "001000" => Data_to_SCUB <= fg_1_data_to_SCUB;
      when "010000" => Data_to_SCUB <= wb_scu_data_to_SCUB;
      when "100000" => Data_to_SCUB <= clk_switch_rd_data;
      when others => Data_to_SCUB <= (others => '0');
    end case;
  end process rd_port_mux;



p_interlock:  
process (AWin1, AWin2, AWin3, AWin4, AWin5,											-- Input-Register
			INL_xor_IO1, INL_xor_IO2, INL_xor_IO3, INL_xor_IO4, INL_xor_IO5,	-- Pegel(xor)-Register (default = 0)
			INL_msk_IO1, INL_msk_IO2, INL_msk_IO3, INL_msk_IO4, INL_msk_IO5,	-- Maskenregister		  (default = 0)
			INL_IO1, 	 INL_IO2,	  INL_IO3,		INL_IO4,		 INL_IO5)		-- Zwischenergebnis: Interlock-Bits xor, Maske
begin
--
	INL_IO1	<=	((AWin1 xor INL_xor_IO1) and not INL_msk_IO1);
	INL_IO2	<=	((AWin2 xor INL_xor_IO2) and not INL_msk_IO2);
	INL_IO3	<=	((AWin3 xor INL_xor_IO3) and not INL_msk_IO3);
	INL_IO4	<=	((AWin4 xor INL_xor_IO4) and not INL_msk_IO4);
	INL_IO5	<=	((AWin5 xor INL_xor_IO5) and not INL_msk_IO5);

-- "Oder-Veknüpfung aller Statusbits (nach Aktiv-Pegel-Selekt und Maske) ---	
	
  interlock <=	(INL_IO1(15) or INL_IO1(14) or INL_IO1(13) or INL_IO1(12) or INL_IO1(11) or INL_IO1(10) or INL_IO1(9) or INL_IO1(8) or
		          INL_IO1(7)  or INL_IO1(6)  or INL_IO1(5)  or INL_IO1(4)  or INL_IO1(3)  or INL_IO1(2)  or INL_IO1(1) or INL_IO1(0) or
		          INL_IO2(15) or INL_IO2(14) or INL_IO2(13) or INL_IO2(12) or INL_IO2(11) or INL_IO2(10) or INL_IO2(9) or INL_IO2(8) or
		          INL_IO2(7)  or INL_IO2(6)  or INL_IO2(5)  or INL_IO2(4)  or INL_IO2(3)  or INL_IO2(2)  or INL_IO2(1) or INL_IO2(0) or
		          INL_IO3(15) or INL_IO3(14) or INL_IO3(13) or INL_IO3(12) or INL_IO3(11) or INL_IO3(10) or INL_IO3(9) or INL_IO3(8) or
		          INL_IO3(7)  or INL_IO3(6)  or INL_IO3(5)  or INL_IO3(4)  or INL_IO3(3)  or INL_IO3(2)  or INL_IO3(1) or INL_IO3(0) or
		          INL_IO4(15) or INL_IO4(14) or INL_IO4(13) or INL_IO4(12) or INL_IO4(11) or INL_IO4(10) or INL_IO4(9) or INL_IO4(8) or
		          INL_IO4(7)  or INL_IO4(6)  or INL_IO4(5)  or INL_IO4(4)  or INL_IO4(3)  or INL_IO4(2)  or INL_IO4(1) or INL_IO4(0) or
		          INL_IO5(15) or INL_IO5(14) or INL_IO5(13) or INL_IO5(12) or INL_IO5(11) or INL_IO5(10) or INL_IO5(9) or INL_IO5(8) or
		          INL_IO5(7)  or INL_IO5(6)  or INL_IO5(5)  or INL_IO5(4)  or INL_IO5(3)  or INL_IO5(2)  or INL_IO5(1) or INL_IO5(0));
end process;
 
  
--  +============================================================================================================================+
--  |                                      		Anwender-IO: P37IO  -- FG900_700		                                             |
--  +============================================================================================================================+


P37IO_in_Start_Deb:  Debounce
  GENERIC MAP (DB_Cnt => C_P37IO_Start_Debounce_input_in_ns / clk_in_ns)    -- Entprellung (C_Debounce_input_in_ns / clk_in_ns)
  PORT MAP  (DB_in => P37IO_Start_i,  Reset => Powerup_Res, clk => clk_sys, DB_Out => P37IO_Start_deb_o);
      
P37IO_Out_Led_Start: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys,   Sig_in => P37IO_Start_deb_o,    nLED => P37IO_nLED_Start_o);
  

P37IO_in_Stop_Deb:  Debounce
  GENERIC MAP (DB_Cnt => C_P37IO_Stop_Debounce_input_in_ns / clk_in_ns)    -- Entprellung (C_Debounce_input_in_ns / clk_in_ns)
  PORT MAP  (DB_in => P37IO_Stop_i,  Reset => Powerup_Res, clk => clk_sys, DB_Out => P37IO_Stop_deb_o);
 
P37IO_Out_Led_Stop: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys,   Sig_in => P37IO_Stop_deb_o,    nLED => P37IO_nLED_Stop_o);
 

P37IO_in_Reset_Deb:  Debounce
  GENERIC MAP (DB_Cnt => C_P37IO_Reset_Debounce_input_in_ns / clk_in_ns)    -- Entprellung (C_Debounce_input_in_ns / clk_in_ns)
  PORT MAP  (DB_in => P37IO_Reset_i,  Reset => Powerup_Res, clk => clk_sys, DB_Out => P37IO_Reset_deb_o);


  
p_P37IO_dff: 
process (clk_sys, P37IO_Start_deb_o, P37IO_Stop_deb_o, P37IO_Reset_deb_o, Powerup_Res)
begin
  -- Reset whenever the reset signal goes low, regardless of the clock
  -- or the clock enable
  if  ( Powerup_Res    = '1') then
			P37IO_BNC_o  <= '0';
  elsif ( (P37IO_Stop_deb_o or P37IO_Reset_deb_o)  = '1') then
			P37IO_BNC_o  <= '0';

      -- If not resetting, and the clock signal is enabled on this register, 
  -- update the register output on the clock's rising edge
  elsif (rising_edge(clk_sys)) then
    if (P37IO_Start_deb_o = '1') then
      P37IO_BNC_o <= '1';
    end if;
  end if;
end process;


P37IO_Out_Led_BNC: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys,   Sig_in => P37IO_BNC_o,    nLED => P37IO_nELD_BNC_o);


P37IO_in_Data_Deb:	for I in 0 to 15 generate
							DB_I:	debounce generic map(DB_Cnt => C_P37IO_Data_Debounce_input_in_ns / clk_in_ns)
							port map(DB_in => P37IO_in_Data_Deb_i(I),  Reset => Powerup_Res, clk => clk_sys, DB_Out => P37IO_in_Data_Deb_o(I));
							end generate P37IO_in_Data_Deb;
							
  

--  +============================================================================================================================+
--  |                                      		Anwender-IO: P25IO  -- FG900_710	                                                |
--  +============================================================================================================================+
  


P25IO_in_Start_Deb:  Debounce
  GENERIC MAP (DB_Cnt => C_P25IO_Start_Debounce_input_in_ns / clk_in_ns)    -- Entprellung (C_Debounce_input_in_ns / clk_in_ns)
  PORT MAP  (DB_in => P25IO_Start_i,  Reset => Powerup_Res, clk => clk_sys, DB_Out => P25IO_Start_deb_o);
      
P25IO_Out_Led_Start: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys,   Sig_in => P25IO_Start_deb_o,    nLED => P25IO_nLED_Start_o);
  

P25IO_in_Stop_Deb:  Debounce
  GENERIC MAP (DB_Cnt => C_P25IO_Stop_Debounce_input_in_ns / clk_in_ns)    -- Entprellung (C_Debounce_input_in_ns / clk_in_ns)
  PORT MAP  (DB_in => P25IO_Stop_i,  Reset => Powerup_Res, clk => clk_sys, DB_Out => P25IO_Stop_deb_o);
 
P25IO_Out_Led_Stop: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys,   Sig_in => P25IO_Stop_deb_o,    nLED => P25IO_nLED_Stop_o);
 

P25IO_in_Reset_Deb:  Debounce
  GENERIC MAP (DB_Cnt => C_P25IO_Reset_Debounce_input_in_ns / clk_in_ns)    -- Entprellung (C_Debounce_input_in_ns / clk_in_ns)
  PORT MAP  (DB_in => P25IO_Reset_i,  Reset => Powerup_Res, clk => clk_sys, DB_Out => P25IO_Reset_deb_o);


  
p_P25IO_dff: 
process (clk_sys, P25IO_Start_deb_o, P25IO_Stop_deb_o, P25IO_Reset_deb_o, Powerup_Res)
begin
  -- Reset whenever the reset signal goes low, regardless of the clock
  -- or the clock enable
  if  ( Powerup_Res    = '1') then
			P25IO_BNC_o  <= '0';
  elsif ( (P25IO_Stop_deb_o or P25IO_Reset_deb_o)  = '1') then
			P25IO_BNC_o  <= '0';

      -- If not resetting, and the clock signal is enabled on this register, 
  -- update the register output on the clock's rising edge
  elsif (rising_edge(clk_sys)) then
    if (P25IO_Start_deb_o = '1') then
      P25IO_BNC_o <= '1';
    end if;
  end if;
end process;


P25IO_Out_Led_BNC: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys,   Sig_in => P25IO_BNC_o,    nLED => P25IO_nELD_BNC_o);


 
 
--------- Puls als Signal (1 Clock breit) --------------------

p_P25IO_DAC_Strobe_Start:  PROCESS (clk_sys, Powerup_Res, P25IO_DAC_Strobe_Start_i)
  BEGin
    IF Powerup_Res  = '1' THEN
      P25IO_DAC_shift  <= (OTHERS => '0');
      P25IO_DAC_Strobe_Start_o    <= '0';

    ELSIF rising_edge(clk_sys) THEN
      P25IO_DAC_shift <= (P25IO_DAC_shift(P25IO_DAC_shift'high-1 downto 0) & (P25IO_DAC_Strobe_Start_i));

      IF P25IO_DAC_shift(P25IO_DAC_shift'high) = '0' AND P25IO_DAC_shift(P25IO_DAC_shift'high-1) = '1' THEN
        P25IO_DAC_Strobe_Start_o <= '1';
      ELSE
        P25IO_DAC_Strobe_Start_o <= '0';
      END IF;
    END IF;
  END PROCESS p_P25IO_DAC_Strobe_Start;
  
  
  --------- DAC_Out-Strobe --------------------
P25IO_DAC_Strobe: led_n
  generic map (stretch_cnt => 125) -- 125 x 8ns(1/125 MHz) = 1us
  port map      (ena => '1', CLK => clk_sys,   Sig_in => P25IO_DAC_Strobe_Start_o,    nLED => P25IO_nDAC_Strobe_o);-- 


   

--------- Puls als Signal (1 Clock breit) --------------------

p_P25IO_ADC_Strobe_Start:  PROCESS (clk_sys, Powerup_Res, P25IO_ADC_ECC_Start_i)
  BEGin
    IF Powerup_Res  = '1' THEN
      P25IO_ADC_shift  <= (OTHERS => '0');
      P25IO_ADC_ECC_Start_o    <= '0';

    ELSIF rising_edge(clk_sys) THEN
      P25IO_ADC_shift <= (P25IO_ADC_shift(P25IO_ADC_shift'high-1 downto 0) & (P25IO_ADC_ECC_Start_i));

      IF P25IO_ADC_shift(P25IO_ADC_shift'high) = '0' AND P25IO_ADC_shift(P25IO_ADC_shift'high-1) = '1' THEN
        P25IO_ADC_ECC_Start_o <= '1';
      ELSE
        P25IO_ADC_ECC_Start_o <= '0';
      END IF;
    END IF;
  END PROCESS p_P25IO_ADC_Strobe_Start;


--------- ADC_Out-ECC --------------------

P25IO_in_Ext_Tim_Deb:  Debounce
  GENERIC MAP (DB_Cnt => C_P25IO_Ext_Tim_Debounce_input_in_ns / clk_in_ns)    -- Entprellung (C_Debounce_input_in_ns / clk_in_ns)
  PORT MAP  (DB_in => P25IO_Ext_Tim_i,  Reset => Powerup_Res, clk => clk_sys, DB_Out => P25IO_Ext_Tim_deb_o);
      
P25IO_Out_Led_Ext_Tim: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys,   Sig_in => P25IO_Ext_Tim_deb_o, nLED => P25IO_nLED_Ext_Tim_o);

  
--------- Puls als Signal (1 Clock breit) --------------------

p_P25IO_Ext_Tim_Strobe_Start:  PROCESS (clk_sys, Powerup_Res, P25IO_Ext_Tim_deb_o)
  BEGin
    IF Powerup_Res  = '1' THEN
      P25IO_Ext_Tim_shift  <= (OTHERS => '0');
      P25IO_Ext_Tim_Strobe_Start_o    <= '0';

    ELSIF rising_edge(clk_sys) THEN
      P25IO_Ext_Tim_shift <= (P25IO_Ext_Tim_shift(P25IO_Ext_Tim_shift'high-1 downto 0) & (P25IO_Ext_Tim_deb_o));

      IF P25IO_Ext_Tim_shift(P25IO_Ext_Tim_shift'high) = '0' AND P25IO_Ext_Tim_shift(P25IO_Ext_Tim_shift'high-1) = '1' THEN
        P25IO_Ext_Tim_Strobe_Start_o <= '1';
      ELSE
        P25IO_Ext_Tim_Strobe_Start_o <= '0';
      END IF;
    END IF;
  END PROCESS p_P25IO_Ext_Tim_Strobe_Start;

  
P25IO_ADC_ECC: led_n
  generic map (stretch_cnt => 250) -- 250 x 8ns(1/125 MHz) = 2us
  port map      (ena => '1', CLK => clk_sys,   Sig_in => (P25IO_ADC_ECC_i), nLED => P25IO_nADC_ECC_o);-- 
  

P25IO_in_EOC_Deb:  Debounce
  GENERIC MAP (DB_Cnt => C_P25IO_EOC_Debounce_input_in_ns / clk_in_ns)    -- Entprellung (C_Debounce_input_in_ns / clk_in_ns)
  PORT MAP  (DB_in => not P25IO_EOC_i,  Reset => Powerup_Res, clk => clk_sys, DB_Out => P25IO_EOC_deb_o);


  
p_P25IO_ADC_FF: 
process (clk_sys, P25IO_EOC_deb_o, P25IO_ADC_Data_FF_i, Powerup_Res)
begin
  if  ( Powerup_Res    	= '1') then	 P25IO_ADC_Data_FF_o  <= (OTHERS => '0');
  elsif (rising_edge(clk_sys)) then
    if (P25IO_EOC_deb_o = '1') then  P25IO_ADC_Data_FF_o  <= P25IO_ADC_Data_FF_i;
    end if;
  end if;
end process;


P25IO_in_Data_Deb:	for I in 0 to 15 generate
							DB_I:	debounce generic map(DB_Cnt => C_OCIO_Data_Debounce_input_in_ns / clk_in_ns)
							port map(DB_in => P25IO_in_Data_Deb_i(I),  Reset => Powerup_Res, clk => clk_sys, DB_Out => P25IO_in_Data_Deb_o(I));
							end generate P25IO_in_Data_Deb;
							
  

--  +============================================================================================================================+
--  |                                      		Anwender-IO: OCIN  -- FG900_720	                                                |
--  +============================================================================================================================+



OCIN_in_Data_Deb1:	for I in 0 to 15 generate
							DB_I:	debounce generic map(DB_Cnt => C_OCIN_Data_Debounce_input_in_ns / clk_in_ns)
							port map(DB_in => OCIN_in_Data_Deb1_i(I),  Reset => Powerup_Res, clk => clk_sys, DB_Out => OCIN_in_Data_Deb1_o(I));
							end generate OCIN_in_Data_Deb1;
							
OCIN_in_Data_Deb2:	for I in 0 to 15 generate
							DB_I:	debounce generic map(DB_Cnt => C_OCIN_Data_Debounce_input_in_ns / clk_in_ns)
							port map(DB_in => OCIN_in_Data_Deb2_i(I),  Reset => Powerup_Res, clk => clk_sys, DB_Out => OCIN_in_Data_Deb2_o(I));
							end generate OCIN_in_Data_Deb2;
							
  

--  +============================================================================================================================+
--  |                                      		Anwender-IO: OCIO  -- FG900_730	                                                |
--  +============================================================================================================================+



OCIO_in_Data_Deb1:	for I in 0 to 15 generate
							DB_I:	debounce generic map(DB_Cnt => C_OCIO_Data_Debounce_input_in_ns / clk_in_ns)
							port map(DB_in => OCIO_in_Data_Deb1_i(I),  Reset => Powerup_Res, clk => clk_sys, DB_Out => OCIO_in_Data_Deb1_o(I));
							end generate OCIO_in_Data_Deb1;

OCIO_in_Data_Deb2:	for I in 0 to 7 generate
							DB_I:	debounce generic map(DB_Cnt => C_OCIO_Data_Debounce_input_in_ns / clk_in_ns)
							port map(DB_in => OCIO_in_Data_Deb2_i(I),  Reset => Powerup_Res, clk => clk_sys, DB_Out => OCIO_in_Data_Deb2_o(I));
							end generate OCIO_in_Data_Deb2;
							
  

--  +============================================================================================================================+
--  |                                      		Anwender-IO:	SPSIO	--  FG900.770 ---                                           |
--  +============================================================================================================================+

SPSIO_in_Data_Deb:	for I in 0 to 23 generate
							DB_I:	debounce generic map(DB_Cnt => C_SPSIO_Data_Debounce_input_in_ns / clk_in_ns)
							port map(DB_in => SPSIO_in_Data_Deb_i(I),  Reset => Powerup_Res, clk => clk_sys, DB_Out => SPSIO_in_Data_Deb_o(I));
							end generate SPSIO_in_Data_Deb;

--  +============================================================================================================================+
--  |                                      		Anwender-IO:	HFIO	--  FG900.780 ---                                           |
--  +============================================================================================================================+


HFIO_nLED_Tastpuls: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys,   Sig_in => HFIO_Tastpuls_i,    nLED => HFIO_nLED_Tastpuls_o);

HFIO_nLED_Sample_Puls_inv: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys,   Sig_in => HFIO_Sample_Puls_inv_i,    nLED => HFIO_nLED_Sample_Puls_inv_o);

HFIO_nLED_Aux: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys,   Sig_in => HFIO_Aux_i,    nLED => HFIO_nLED_Aux_o);

HFIO_nLED_Sample_Puls_Display: led_n
  generic map (stretch_cnt => stretch_cnt)
  port map      (ena => Ena_Every_20ms, CLK => clk_sys,   Sig_in => HFIO_Sample_Puls_Display_i,    nLED => HFIO_nLED_Sample_Puls_Display_o);



HFIO_in_AMP_FEHLER_Deb:  Debounce
  GENERIC MAP (DB_Cnt => C_HFIO_AMP_FEHLER_Debounce_input_in_ns / clk_in_ns)    -- Entprellung (C_Debounce_input_in_ns / clk_in_ns)
  PORT MAP  (DB_in => HFIO_in_AMP_FEHLER_Deb_i,  Reset => Powerup_Res, clk => clk_sys, DB_Out => HFIO_in_AMP_FEHLER_Deb_o);
 
HFIO_in_PHASE_FEHLER_Deb:  Debounce
  GENERIC MAP (DB_Cnt => C_HFIO_PHASE_FEHLER_Debounce_input_in_ns / clk_in_ns)    -- Entprellung (C_Debounce_input_in_ns / clk_in_ns)
  PORT MAP  (DB_in => HFIO_in_PHASE_FEHLER_Deb_i,  Reset => Powerup_Res, clk => clk_sys, DB_Out => HFIO_in_PHASE_FEHLER_Deb_o);
 
 
  
  

--  +============================================================================================================================+
--  +============================================================================================================================+
  
  

p_AW_MUX: PROCESS (clk_sys, Powerup_Res, Powerup_Done, s_AW_ID, s_nLED_Out, PIO, A_SEL,
						 fg_1_sw, fg_1_strobe, P25IO_DAC_Out,
                   AW_Config, CLK_IO, AWin1, AWin2, AWin3, AWin4, AWin5, AWin6, AWin7, 
                   AWOut_Reg1, AWOut_Reg1_wr, AWOut_Reg2, AWOut_Reg2_wr, AWOut_Reg3, AWOut_Reg4, AWOut_Reg5, AWOut_Reg6, AWOut_Reg7, 
                   Interlock, UIO, hp_la_o, local_clk_is_running, clk_blink,
						 s_nLED_Sel, s_nLED_Dtack, s_nLED_inR, s_nLED_User1_o, s_nLED_User2_o, s_nLED_User3_o, 
						 P37IO_nLED_Start_o, P37IO_nLED_Stop_o, P37IO_BNC_o, P37IO_nELD_BNC_o, P37IO_in_Data_Deb_o,
						 P25IO_EOC_deb_o, P25IO_nLED_Start_o, P25IO_nLED_Stop_o, P25IO_BNC_o, P25IO_nELD_BNC_o,
						 P25IO_nDAC_Strobe_o, P25IO_nLED_Ext_Tim_o, P25IO_nADC_ECC_o, P25IO_in_Data_Deb_o, P25IO_ADC_Data_FF_o,
					    P25IO_ADC_Data_FF_i, P25IO_ADC_ECC_Start_o, P25IO_Ext_Tim_Strobe_Start_o,
						 OCIN_in_Data_Deb1_o, OCIN_in_Data_Deb2_o,
						 OCIO_in_Data_Deb1_o, OCIO_in_Data_Deb2_o,
						 SPSIO_in_Data_Deb_o,
						 HFIO_nLED_Aux_o, HFIO_nLED_Tastpuls_o, HFIO_nLED_Sample_Puls_Display_o,
						 HFIO_in_AMP_FEHLER_Deb_o, HFIO_in_PHASE_FEHLER_Deb_o, HFIO_nLED_Sample_Puls_inv_o 
                   )
  
BEGIN



  --############################# Set Defaults ######################################

    PIO(150 downto 16)  <= (OTHERS => 'Z'); -- setze alle IO-Pins auf input;
	 UIO(15 downto 0)  	<= (OTHERS => 'Z'); 				-- UIO = Input;

    AWin1(15 downto 0)  <=  x"0000";  -- AW-in-Register 1
    AWin2(15 downto 0)  <=  x"0000";  -- AW-in-Register 2
    AWin3(15 downto 0)  <=  x"0000";  -- AW-in-Register 3
    AWin4(15 downto 0)  <=  x"0000";  -- AW-in-Register 4
    AWin5(15 downto 0)  <=  x"0000";  -- AW-in-Register 5
    AWin6(15 downto 0)  <=  x"0000";  -- AW-in-Register 6
    AWin7(15 downto 0)  <=  x"0000";  -- AW-in-Register 7
    s_AW_ID(7 downto 0) <=  x"FF";    -- Anwender-Karten ID
    
    extension_cid_system <= 0;  -- extension card: cid_system
    extension_cid_group  <= 0;  -- extension card: cid_group

	 s_nLED_User1_i <= '0';				-- LED3 = User 1, -- frei -- 
	 s_nLED_User2_i <= '0';				-- LED3 = User 2, -- frei -- 
	 s_nLED_User3_i <= '0';				-- LED3 = User 3, -- frei -- 
	 
    P37IO_Start_i         <= '0';
    P37IO_Stop_i          <= '0';
    P37IO_Reset_i         <= '0';
    P37IO_in_Data_Deb_i   <= (OTHERS => '0');
    
    P25IO_Start_i            <= '0';
    P25IO_Stop_i             <= '0';
    P25IO_Reset_i            <= '0';
    P25IO_DAC_Strobe_Start_i <= '0';
    P25IO_DAC_Out            <=  (OTHERS => '0');
    P25IO_ADC_ECC_i          <= '0';
    P25IO_ADC_ECC_Start_i    <= '0';
    P25IO_Ext_Tim_i          <= '0';
    P25IO_in_Data_Deb_i      <=  (OTHERS => '0');
    P25IO_ADC_Data_FF_i      <=  (OTHERS => '0');
    P25IO_EOC_i              <= '0';
    
    OCIN_in_Data_Deb1_i      <=  (OTHERS => '0');
    OCIN_in_Data_Deb2_i      <=  (OTHERS => '0');
    OCIO_in_Data_Deb1_i      <=  (OTHERS => '0');
    OCIO_in_Data_Deb2_i      <=  (OTHERS => '0');
                        
    SPSIO_in_Data_Deb_i      <=  (OTHERS => '0');
    
    HFIO_Aux_i						  <= '0';
    HFIO_Tastpuls_i             <= '0';
    HFIO_Sample_Puls_Display_i  <= '0';
    HFIO_Sample_Puls_inv_i      <= '0';
    HFIO_in_AMP_FEHLER_Deb_i	  <= '0';    	 
	 HFIO_in_PHASE_FEHLER_Deb_i  <= '0';    	 
  
	 
    --#################################################################################
    --###                                                                           ###
    --###                    IO-Stecker-Test mit "BrückenStecker                    ###
    --###                                                                           ###
    --#################################################################################
    
    
    IF  AW_Config(15) = '1'  THEN   -- Config-Reg Bit15 = 1  --> Testmode 
--  IF  A_SEL = not (X"F")   THEN   -- Codierschalter = x"F" --> Testmode 


		--- Test der PIO-Pins ---

      AWin1(15 downto 0)  <=  ( CLK_IO,   PIO(16),  PIO(17),  PIO(18),  PIO(19),  PIO(20),  PIO(21),  PIO(22),
                                PIO(23),  PIO(24),  PIO(25),  PIO(26),  PIO(27),  PIO(28),  PIO(29),  PIO(30)   );

          ( PIO(61),  PIO(62),  PIO(59),  PIO(60),  PIO(57),  PIO(58),  PIO(55),  PIO(56),                      
            PIO(53),  PIO(54),  PIO(51),  PIO(52),  PIO(49),  PIO(50),  PIO(47),  PIO(48)   )   <=  AWOut_Reg1(15 downto 0) ;         
  

      AWin2(15 downto 0)  <=  ( PIO(31),  PIO(32),  PIO(33),  PIO(34),  PIO(35),  PIO(36),  PIO(37),  PIO(38),
                                PIO(39),  PIO(40),  PIO(41),  PIO(42),  PIO(43),  PIO(44),  PIO(45),  PIO(46)   );

          ( PIO(77),  PIO(78),  PIO(75),  PIO(76),  PIO(73),  PIO(74),  PIO(71),  PIO(72),
            PIO(69),  PIO(70),  PIO(67),  PIO(68),  PIO(65),  PIO(66),  PIO(63),  PIO(64)   )   <=  AWOut_Reg2(15 downto 0) ;


      AWin3(15 downto 0)  <=  ( PIO(79),  PIO(80),  PIO(81),  PIO(82),  PIO(83),  PIO(84),  PIO(85),  PIO(86),
                                PIO(87),  PIO(88),  PIO(89),  PIO(90),  PIO(91),  PIO(92),  PIO(93),  PIO(94)   );
              
          ( PIO(125), PIO(126), PIO(123), PIO(124), PIO(121), PIO(122), PIO(119), PIO(120),
            PIO(117), PIO(118), PIO(115), PIO(116), PIO(113), PIO(114), PIO(111), PIO(112)  )   <=  AWOut_Reg3(15 downto 0) ;

            
      AWin4(15 downto 0)  <=  ( PIO(95),  PIO(96),  PIO(97),  PIO(98),  PIO(99),  PIO(100), PIO(101), PIO(102),
                                PIO(103), PIO(104), PIO(105), PIO(106), PIO(107), PIO(108), PIO(109), PIO(110)  );
                          
          ( PIO(141), PIO(142), PIO(139), PIO(140), PIO(137), PIO(138), PIO(135), PIO(136),               
            PIO(133), PIO(134), PIO(131), PIO(132), PIO(129), PIO(130), PIO(127), PIO(128)  )   <=  AWOut_Reg4(15 downto 0) ;



		AWin5(15 downto 4)  <=   AWOut_Reg5(15 downto 4); --+   Input [15..4] = Copy der Output-Bits, da Testprog. nur 16 Bit Vergleich.
 		AWin5(3  downto 0)  <=  (PIO(143), PIO(144), PIO(149), PIO(150));
 
										(PIO(147), PIO(148), PIO(145), PIO(146))    <=  AWOut_Reg5(3 downto 0) ;


		--- Test der User-Pins zur VG-Leiste und HPLA1 (HP-Logicanalysator) ---

		UIO(15 downto 0)  	<= (OTHERS => 'Z'); 				-- UIO = Input;
		AWin6(15 downto 0)	<=	UIO(15 downto 0); 			-- User-Pins zur VG-Leiste als Input
		A_TA(15 downto 0)		<= AWOut_Reg6(15 downto 0);	-- HPLA1 (HP-Logicanalysator) als Output


		--- Test Codierschalter ---

		AWin7(15 downto 4)	<=	(OTHERS => '0'); 			-- setze alle unbenutzten Bit's = 0
		AWin7(3 downto 0)		<=	not A_SEL(3 downto 0); 	-- Lese Codierschalter (neg. Logic)


 

  else
  
    --#################################################################################
    --#################################################################################
    --###                                                                           ###
    --###                         Stecker Anwender I/O                              ###
    --###                                                                           ###
    --#################################################################################
    --#################################################################################
  
  

    --input: Anwender_ID ---      
      s_AW_ID(7 downto 0)   <=  PIO(150 downto 143);
      AWin5(15 downto 0)    <=  x"00" & s_AW_ID(7 downto 0);-- Anwender_ID
    

	
    --  --- Output: Anwender-LED's ---

		PIO(17) <= s_nLED_Sel;        						-- LED7 = sel Board 
      PIO(19) <= s_nLED_Dtack;      						-- LED6 = Dtack 
      PIO(21) <= s_nLED_inR;        						-- LED5 = interrupt
      PIO(23) <= not Powerup_Done or clk_blink;			-- LED4 = Powerup 
      PIO(25) <= s_nLED_User1_o;								-- LED3 = User 1
      PIO(27) <= s_nLED_User2_o;								-- LED2 = User 2  
      PIO(29) <= s_nLED_User3_o;								-- LED1 = User 3 
      PIO(31) <= local_clk_is_running and clk_blink;  -- LED0 (User-4) = int. Clock 

		
		A_TA		<= hp_la_o; ----- Output für HP-Logicanalysator

		UIO(0)	<= not Interlock; -- Ist kein Interlock-Bit gesetzt ==> UIO(0) = 1 (low-aktiv)
    
  CASE s_AW_ID(7 downto 0) IS
  

  WHEN  c_AW_P37IO =>

    --#################################################################################
    --####                  Anwender-IO: P37IO  -- FG900_700                        ###
    --#################################################################################

      extension_cid_system <= 55; -- extension card: cid_system, CSCOHW=55
      extension_cid_group  <= 27; -- extension card: cid_group, "FG900700_P37IO1" = 27
    

		s_nLED_User1_i <= '0';				-- LED3 = User 1, -- frei -- 
		s_nLED_User2_i <= '0';				-- LED3 = User 2, -- frei -- 
		s_nLED_User3_i <= '0';				-- LED3 = User 3, -- frei -- 

		
    --############################# Start/Stop FF ######################################

		P37IO_Start_i		<=	not PIO(139);       		-- input "LemoBuchse-Start" H-Aktiv, nach dem Optokoppler aber L-Aktiv
      PIO(33)   			<= P37IO_nLED_Start_o;   --	Output "nLED_Start"
      P37IO_Stop_i		<=	not PIO(141);         	-- input "LemoBuchse-Stop" L-Aktiv, nach dem Optokoppler aber L-Aktiv
      PIO(35)   			<= P37IO_nLED_Stop_o;		--	Output "nLED_Stop"
      P37IO_Reset_i		<=	not PIO(133);         	-- input "Rest-Taster" L-Aktiv

      PIO(51)       		<=  P37IO_BNC_o;        	-- Output "BNC"
      PIO(37)       		<=  P37IO_nELD_BNC_o;    -- Output "nLED_BNC"


      PIO(39) <=  '0';  ------------------------------- Output_Enable (nach init vom ALTERA)
      PIO(41) <=  '0';  ------------------------------- Output_Enable (nach init vom ALTERA)
      PIO(43) <=  '0';  ------------------------------- Output_Enable (nach init vom ALTERA)

      PIO(65) <=  not AWOut_Reg1(7);  --  Output "CO_D7"
      PIO(69) <=  not AWOut_Reg1(6);  --  Output "CO_D6"
      PIO(61) <=  not AWOut_Reg1(5);  --  Output "CO_D5"
      PIO(67) <=  not AWOut_Reg1(4);  --  Output "CO_D4"
      PIO(63) <=  not AWOut_Reg1(3);  --  Output "CO_D3"
      PIO(71) <=  not AWOut_Reg1(2);  --  Output "CO_D2"
      PIO(55) <=  not AWOut_Reg1(1);  --  Output "CO_D1"
      PIO(53) <=  not AWOut_Reg1(0);  --  Output "CO_D0"
      PIO(57) <=  not AWOut_Reg2(1);  --  Output "CO_FAULT"
      PIO(59) <=  not AWOut_Reg2(0);  --  Output "CO_STAT"
                

					

    --############################# Debounce Input ######################################
			
      P37IO_in_Data_Deb_i(15) <=  PIO(131); --  input "HI7"
      P37IO_in_Data_Deb_i(14) <=  PIO(129); --  input "HI6"
      P37IO_in_Data_Deb_i(13) <=  PIO(127); --  input "HI5"
      P37IO_in_Data_Deb_i(12) <=  PIO(125); --  input "HI4"
      P37IO_in_Data_Deb_i(11) <=  PIO(123); --  input "HI3"
      P37IO_in_Data_Deb_i(10) <=  PIO(121); --  input "HI2"
      P37IO_in_Data_Deb_i(9)  <=  PIO(119); --  input "HI1"
      P37IO_in_Data_Deb_i(8)  <=  PIO(117); --  input "HI0"
    
      P37IO_in_Data_Deb_i(7)  <=  PIO(115); --  input "LO7"
      P37IO_in_Data_Deb_i(6)  <=  PIO(113); --  input "LO6"
      P37IO_in_Data_Deb_i(5)  <=  PIO(111); --  input "LO5"
      P37IO_in_Data_Deb_i(4)  <=  PIO(109); --  input "LO4"
      P37IO_in_Data_Deb_i(3)  <=  PIO(107); --  input "LO3"
      P37IO_in_Data_Deb_i(2)  <=  PIO(105); --  input "LO2"
      P37IO_in_Data_Deb_i(1)  <=  PIO(103); --  input "LO1"
      P37IO_in_Data_Deb_i(0)  <=  PIO(101); --  input "LO0"
		
		AWin1	<=	P37IO_in_Data_Deb_o;



  WHEN   c_AW_P25IO =>
  
    --#################################################################################
    --####                    Anwender-IO: P25IO  -- FG900_710                      ###
    --#################################################################################

		extension_cid_system <= 55; -- extension card: cid_system, CSCOHW=55
		extension_cid_group  <= 28; -- extension card: cid_group, "FG900710_P25IO1" = 28

		
      s_nLED_User1_i <= AW_Config(1);				-- LED3 = User 1, DAC-Data vom FG
      s_nLED_User2_i <= not P25IO_nADC_ECC_o;	-- LED2 = User 2, ECC zum ADC (Enable)  
      s_nLED_User3_i <= P25IO_EOC_deb_o;			-- LED1 = User 3, EOC vom ADC 
		
		
 
    --############################# Start/Stop FF ######################################

		P25IO_Start_i		<=	not PIO(71);       		-- input "LemoBuchse-Start" H-Aktiv, nach dem Optokoppler aber L-Aktiv
      PIO(87)   			<= P25IO_nLED_Start_o;   	--	Output "nLED_Start"
      P25IO_Stop_i		<=	not PIO(75);         	-- input "LemoBuchse-Stop" L-Aktiv, nach dem Optokoppler aber L-Aktiv
      PIO(89)   			<= P25IO_nLED_Stop_o;		--	Output "nLED_Stop"
      P25IO_Reset_i		<=	not PIO(67);         	-- input "Rest-Taster" L-Aktiv

      PIO(103)      		<=  P25IO_BNC_o;        	-- Output "BNC"
      PIO(91)       		<=  P25IO_nELD_BNC_o;    	-- Output "nLED_BNC"
 

    --############################# DAC out ######################################

		IF  (AW_Config(1) = '1')  THEN
  
--           FG_mode; DAC-Werte kommen von FG_Data und werden mit FG_Strobe uebernommen. Kein externer Trigger! 

				P25IO_DAC_Strobe_Start_i	<=	fg_1_strobe;  				-- fg_1_strobe (vom Funktionsgen)
				PIO(105)							<=	P25IO_nDAC_Strobe_o;		--	Der Strobe-Output ist "LO"-Aktiv
            P25IO_DAC_Out(15 downto 0) <= fg_1_sw(31 downto 16);  
        Else
--           Software-Mode, DAC-Werte, kommen vom SCU-Bus-Slave. Externe Triggerung mit pos. oder neg. Flanke, kann eingeschaltet werden. 

				P25IO_DAC_Strobe_Start_i	<=	AWOut_Reg1_wr;				-- AWOut_Reg1_wr (vom SCU-Bus-Slave)
				PIO(105)							<=	P25IO_nDAC_Strobe_o;		--	Der Strobe-Output ist "LO"-Aktiv	
            P25IO_DAC_Out(15 downto 0) <= AWOut_Reg1(15 downto 0);
      END IF; 


		--		Output DAC-Daten

      PIO(77) <=  '0';  ------------------------------- Output_Enable (nach init vom ALTERA)
      PIO(79) <=  '0';  ------------------------------- Output_Enable (nach init vom ALTERA)
      PIO(81) <=  '0';  ------------------------------- Output_Enable (nach init vom ALTERA)
      PIO(83) <=  '0';  ------------------------------- Output_Enable (nach init vom ALTERA)
      PIO(95) <=  '0';  ------------------------------- Output_Enable (nach init vom ALTERA)


      PIO(107)  <=  not P25IO_DAC_Out(15);  -- Output Bit-15
      PIO(109)  <=  not P25IO_DAC_Out(14);  -- Output Bit-14
      PIO(111)  <=  not P25IO_DAC_Out(13);  -- Output Bit-13
      PIO(113)  <=  not P25IO_DAC_Out(12);  -- Output Bit-12
      PIO(115)  <=  not P25IO_DAC_Out(11);  -- Output Bit-11
      PIO(117)  <=  not P25IO_DAC_Out(10);  -- Output Bit-10
      PIO(119)  <=  not P25IO_DAC_Out(9);   -- Output Bit-9
      PIO(121)  <=  not P25IO_DAC_Out(8);   -- Output Bit-8
      PIO(123)  <=  not P25IO_DAC_Out(7);   -- Output Bit-7
      PIO(125)  <=  not P25IO_DAC_Out(6);   -- Output Bit-6
      PIO(127)  <=  not P25IO_DAC_Out(5);   -- Output Bit-5
      PIO(129)  <=  not P25IO_DAC_Out(4);   -- Output Bit-4
      PIO(131)  <=  not P25IO_DAC_Out(3);   -- Output Bit-3
      PIO(133)  <=  not P25IO_DAC_Out(2);   -- Output Bit-2
      PIO(135)  <=  not P25IO_DAC_Out(1);   -- Output Bit-1
      PIO(137)  <=  not P25IO_DAC_Out(0);   -- Output Bit-0
		


    --############################# ADC in ######################################


--		Start ADC-Conversion		
		P25IO_ADC_ECC_Start_i	<=	AWOut_Reg2_wr;													-- pos. Flanke vom AWOut_Reg2_wr (vom SCU-Bus-Slave) ==> Puls von 1 Clock-Breite.
		P25IO_ADC_ECC_i			<=	((P25IO_ADC_ECC_Start_o and AWOut_Reg2(0)) or 		-- wenn AWOut_Reg2(0)=1 ist oder  
											  P25IO_Ext_Tim_Strobe_Start_o);							-- ein ext. Trigger ==> Puls = 2us 

		P25IO_Ext_Tim_i			<=	not PIO(85);       		-- input "LemoBuchse-ExtTiming" H-Aktiv, nach dem Optokoppler aber L-Aktiv
      PIO(93)   					<= P25IO_nLED_Ext_Tim_o;   --	Output "nLED_Extern-Timing"

		PIO(101)						<=	not P25IO_nADC_ECC_o;	--	Der Strobe-Output für ECC "Hi"-Aktiv
		

--		input ADC-Daten
		P25IO_in_Data_Deb_i(15)	<=	  PIO(41);  --------------  input "HI7"      
		P25IO_in_Data_Deb_i(14)	<=   PIO(43);  --------------  input "HI6"     
		P25IO_in_Data_Deb_i(13)	<=   PIO(37);  --------------  input "HI5"     
		P25IO_in_Data_Deb_i(12)	<=   PIO(39);  --------------  input "HI4"     
		P25IO_in_Data_Deb_i(11)	<=   PIO(33);  --------------  input "HI3"     
		P25IO_in_Data_Deb_i(10)	<=   PIO(35);  --------------  input "HI2"     
		P25IO_in_Data_Deb_i(9)	<=   PIO(49);  --------------  input "HI1"    
		P25IO_in_Data_Deb_i(8)	<=   PIO(51);  --------------  input "HI0"    
		P25IO_in_Data_Deb_i(7)	<=   PIO(53);  --  input "Lo7"    
		P25IO_in_Data_Deb_i(6)	<=   PIO(55);  --  input "LO6"    
		P25IO_in_Data_Deb_i(5)	<=   PIO(45);  --  input "LO5"    
		P25IO_in_Data_Deb_i(4)	<=   PIO(47);  --  input "LO4"    
		P25IO_in_Data_Deb_i(3)	<=   PIO(57);  --  input "LO3"    
		P25IO_in_Data_Deb_i(2)	<=   PIO(59);  --  input "LO2"    
		P25IO_in_Data_Deb_i(1)	<=   PIO(61);  --  input "LO1"    
		P25IO_in_Data_Deb_i(0)	<=   PIO(63);  --  input "LO0"    
			
		P25IO_ADC_Data_FF_i		<=	P25IO_in_Data_Deb_o;

		
--		ADC-EOC
		P25IO_EOC_i		<=		PIO(65); -- input Strobe für ADC-Daten

	
		
    IF  (AW_Config(2) = '1')  THEN
  
--          ADC-Daten aus dem Speicher (gespeichert mit EOC) 
				AWin1		<=		P25IO_ADC_Data_FF_o;     -- Output "Daten ADC-Register"
       Else
--           ADC-Daten die am Sub-D Stecker anstehen. 
				AWin1		<=		P25IO_ADC_Data_FF_i;     -- Output "Daten ADC-Register"
      END IF; 

 



 
  
	WHEN   c_AW_OCin =>

    --#################################################################################
    --####                    Anwender-IO: OCIN -- FG900_720                        ###
    --#################################################################################

      extension_cid_system <= 55; -- extension card: cid_system, CSCOHW=55
      extension_cid_group  <= 29; -- extension card: cid_group, "FG900720_OCin1" = 29


		s_nLED_User1_i <= '0';				-- LED3 = User 1, -- frei -- 
		s_nLED_User2_i <= '0';				-- LED3 = User 2, -- frei -- 
		s_nLED_User3_i <= '0';				-- LED3 = User 3, -- frei -- 
		
		
		

      OCIN_in_Data_Deb1_i(15) <=  '0';      --------------  Frei
      OCIN_in_Data_Deb1_i(14) <=  '0';      --------------  Frei
      OCIN_in_Data_Deb1_i(13) <=  PIO(73);  --------------  input "B5"
      OCIN_in_Data_Deb1_i(12) <=  PIO(75);  --------------  input "B4"
      OCIN_in_Data_Deb1_i(11) <=  PIO(93);  --------------  input "B3"
      OCIN_in_Data_Deb1_i(10) <=  PIO(95);  --------------  input "B2"
      OCIN_in_Data_Deb1_i(9)  <=  PIO(97);  --------------  input "B1"
      OCIN_in_Data_Deb1_i(8)  <=  PIO(99);  --------------  input "B0"
      OCIN_in_Data_Deb1_i(7)  <=  PIO(117); --  input "A7"
      OCIN_in_Data_Deb1_i(6)  <=  PIO(119); --  input "A6"
      OCIN_in_Data_Deb1_i(5)  <=  PIO(121); --  input "A5"
      OCIN_in_Data_Deb1_i(4)  <=  PIO(123); --  input "A4"
      OCIN_in_Data_Deb1_i(3)  <=  PIO(133); --  input "A3"
      OCIN_in_Data_Deb1_i(2)  <=  PIO(135); --  input "A2"
      OCIN_in_Data_Deb1_i(1)  <=  PIO(137); --  input "A1"
      OCIN_in_Data_Deb1_i(0)  <=  PIO(139); --  input "A0"
		
		AWin1		<=		OCIN_in_Data_Deb1_o;
		
  
      OCIN_in_Data_Deb2_i(15) <=  PIO(81);  --------------  input "D7"
      OCIN_in_Data_Deb2_i(14) <=  PIO(79);  --------------  input "D6"
      OCIN_in_Data_Deb2_i(13) <=  PIO(77);  --------------  input "D5"
      OCIN_in_Data_Deb2_i(12) <=  PIO(83);  --------------  input "D4"
      OCIN_in_Data_Deb2_i(11) <=  PIO(85);  --------------  input "D3"
      OCIN_in_Data_Deb2_i(10) <=  PIO(87);  --------------  input "D2"
      OCIN_in_Data_Deb2_i(9)  <=  PIO(89);  --------------  input "D1"
      OCIN_in_Data_Deb2_i(8)  <=  PIO(91);  --------------  input "D0"
      OCIN_in_Data_Deb2_i(7)  <=  PIO(109); --  input "C7"
      OCIN_in_Data_Deb2_i(6)  <=  PIO(111); --  input "C6"
      OCIN_in_Data_Deb2_i(5)  <=  PIO(113); --  input "C5"
      OCIN_in_Data_Deb2_i(4)  <=  PIO(115); --  input "C4"
      OCIN_in_Data_Deb2_i(3)  <=  PIO(125); --  input "C3"
      OCIN_in_Data_Deb2_i(2)  <=  PIO(127); --  input "C2"
      OCIN_in_Data_Deb2_i(1)  <=  PIO(129); --  input "C1"
      OCIN_in_Data_Deb2_i(0)  <=  PIO(131); --  input "C0"

		AWin2		<=		OCIN_in_Data_Deb2_o;
   
    
      PIO(39)   <=  '0';  ------------------------------- Output_Enable (nach init vom ALTERA)

      PIO(49) <=  not AWOut_Reg1(3);  --  Output "2CB2"
      PIO(47) <=  not AWOut_Reg1(2);  --  Output "2CA2"
      PIO(45) <=  not AWOut_Reg1(1);  --  Output "1CB2"
      PIO(43) <=  not AWOut_Reg1(0);  --  Output "1CA2"


    
  WHEN   c_AW_OCIO =>
  
    --#################################################################################
    --####                      Anwender-IO: OCIO -- FG900_730                      ###
    --#################################################################################

      extension_cid_system <= 55; -- extension card: cid_system, CSCOHW=55
      extension_cid_group  <= 30; -- extension card: cid_group, "FG900730_OCIO1" = 30

    
		s_nLED_User1_i <= '0';				-- LED3 = User 1, -- frei -- 
		s_nLED_User2_i <= '0';				-- LED3 = User 2, -- frei -- 
		s_nLED_User3_i <= '0';				-- LED3 = User 3, -- frei -- 
    

      OCIO_in_Data_Deb1_i(15) <=  PIO(45);  --------------  input "C7"
      OCIO_in_Data_Deb1_i(14) <=  PIO(47);  --------------  input "C6"
      OCIO_in_Data_Deb1_i(13) <=  PIO(51);  --------------  input "C5"
      OCIO_in_Data_Deb1_i(12) <=  PIO(53);  --------------  input "C4"
      OCIO_in_Data_Deb1_i(11) <=  PIO(37);  --------------  input "C3"
      OCIO_in_Data_Deb1_i(10) <=  PIO(39);  --------------  input "C2"
      OCIO_in_Data_Deb1_i(9)  <=  PIO(41);  --------------  input "C1"
      OCIO_in_Data_Deb1_i(8)  <=  PIO(43);  --------------  input "C0"
      OCIO_in_Data_Deb1_i(7)  <=  PIO(127); --  input "A7"
      OCIO_in_Data_Deb1_i(6)  <=  PIO(125); --  input "A6"
      OCIO_in_Data_Deb1_i(5)  <=  PIO(123); --  input "A5"
      OCIO_in_Data_Deb1_i(4)  <=  PIO(121); --  input "A4"
      OCIO_in_Data_Deb1_i(3)  <=  PIO(99);  --  input "A3"
      OCIO_in_Data_Deb1_i(2)  <=  PIO(97);  --  input "A2"
      OCIO_in_Data_Deb1_i(1)  <=  PIO(95);  --  input "A1"
      OCIO_in_Data_Deb1_i(0)  <=  PIO(93);  --  input "A0"

		AWin1		<=		OCIO_in_Data_Deb1_o;
		
		
      OCIO_in_Data_Deb2_i(7)  <=  PIO(89);  --  input "D7"		
      OCIO_in_Data_Deb2_i(6)  <=  PIO(91);  --  input "D6"
      OCIO_in_Data_Deb2_i(5)  <=  PIO(119); --  input "D5"
      OCIO_in_Data_Deb2_i(4)  <=  PIO(117); --  input "D4"
      OCIO_in_Data_Deb2_i(3)  <=  PIO(87);  --  input "D3"
      OCIO_in_Data_Deb2_i(2)  <=  PIO(85);  --  input "D2"
      OCIO_in_Data_Deb2_i(1)  <=  PIO(83);  --  input "D1"
      OCIO_in_Data_Deb2_i(0)  <=  PIO(81);  --  input "D0"

      AWin2(15 downto 8)  <=  (OTHERS => '0');  -- inPUT = 0; 
      AWin2(15 downto 8)  <=  OCIO_in_Data_Deb2_o;  
  

      PIO(77)   <=  '0';  ------------------------------- Output_Enable (nach init vom ALTERA)
      
      PIO(105)  <=  not AWOut_Reg1(11); ----------------  Output "CD2"
      PIO(61)   <=  not AWOut_Reg1(10); ----------------  Output "CC2"
      PIO(107)  <=  not AWOut_Reg1(9);  ----------------  Output "CB2"
      PIO(115)  <=  not AWOut_Reg1(8);  ----------------  Output "CA2"
      PIO(109)  <=  not AWOut_Reg1(7);  --  Output "B7"
      PIO(111)  <=  not AWOut_Reg1(6);  --  Output "B6"
      PIO(113)  <=  not AWOut_Reg1(5);  --  Output "B5"
      PIO(101)  <=  not AWOut_Reg1(4);  --  Output "B4"
      PIO(103)  <=  not AWOut_Reg1(3);  --  Output "B3"
      PIO(59)   <=  not AWOut_Reg1(2);  --  Output "B2"
      PIO(57)   <=  not AWOut_Reg1(1);  --  Output "B1"
      PIO(55)   <=  not AWOut_Reg1(0);  --  Output "B0"


      
  WHEN   c_AW_UIO =>
  
    --###################################################################################
    --####                      Anwender-IO: UIO  -- FG900_740                        ###
    --###################################################################################



  WHEN   c_AW_DA =>
      
    --###################################################################################
    --####                  Anwender-IO: DA(DAC/ADC)  -- FG900_750                    ###
    --###################################################################################



  WHEN   c_AW_Frei =>
      
    --###################################################################################
    --####                  Anwender-IO: DA(DAC/ADC)  -- FG900_760                    ###
    --###################################################################################



  WHEN   c_AW_SPSIO =>
      
    --###################################################################################
    --####                  Anwender-IO: DA(DAC/ADC)  -- FG900_770                    ###
    --###################################################################################

		extension_cid_system <= 55; -- extension card: cid_system, CSCOHW=55
      extension_cid_group  <= 35; -- extension card: cid_group, "FG900770_SPSIO1" = 34

		s_nLED_User1_i <= '0';				-- LED3 = User 1, -- frei -- 
		s_nLED_User2_i <= '0';				-- LED3 = User 2, -- frei -- 
		s_nLED_User3_i <= '0';				-- LED3 = User 3, -- frei -- 



	--========================== Input Register 1+2 ======================================

      SPSIO_in_Data_Deb_i(23) <=  PIO(35); --  input "23"
      SPSIO_in_Data_Deb_i(22) <=  PIO(37); --  input "22"
      SPSIO_in_Data_Deb_i(21) <=  PIO(39); --  input "21"
      SPSIO_in_Data_Deb_i(20) <=  PIO(41); --  input "20"
      SPSIO_in_Data_Deb_i(19) <=  PIO(43); --  input "19"
      SPSIO_in_Data_Deb_i(18) <=  PIO(45); --  input "18"
      SPSIO_in_Data_Deb_i(17) <=  PIO(47); --  input "17"
      SPSIO_in_Data_Deb_i(16) <=  PIO(49); --  input "16"
      SPSIO_in_Data_Deb_i(15) <=  PIO(51); --  input "15"
      SPSIO_in_Data_Deb_i(14) <=  PIO(53); --  input "14"
      SPSIO_in_Data_Deb_i(13) <=  PIO(55); --  input "13"
      SPSIO_in_Data_Deb_i(12) <=  PIO(57); --  input "12"
      SPSIO_in_Data_Deb_i(11) <=  PIO(59); --  input "11"
      SPSIO_in_Data_Deb_i(10) <=  PIO(61); --  input "10"
      SPSIO_in_Data_Deb_i(9)  <=  PIO(63); --  input "9"
      SPSIO_in_Data_Deb_i(8)  <=  PIO(65); --  input "8"
      SPSIO_in_Data_Deb_i(7)  <=  PIO(67); --  input "7"
      SPSIO_in_Data_Deb_i(6)  <=  PIO(69); --  input "6"
      SPSIO_in_Data_Deb_i(5)  <=  PIO(71); --  input "5"
      SPSIO_in_Data_Deb_i(4)  <=  PIO(73); --  input "4"
      SPSIO_in_Data_Deb_i(3)  <=  PIO(75); --  input "3"
      SPSIO_in_Data_Deb_i(2)  <=  PIO(77); --  input "2"
      SPSIO_in_Data_Deb_i(1)  <=  PIO(79); --  input "1"
      SPSIO_in_Data_Deb_i(0)  <=  PIO(81); --  input "0"


		AWin1(15 downto 0)		<=		SPSIO_in_Data_Deb_o(15 downto 0);
		AWin2(15 downto 8)		<=		(OTHERS => '0');  -- InPUT = 0; 
		AWin2(7  downto 0)		<=		SPSIO_in_Data_Deb_o(23 downto 16);
		


	--========================== Output Register 1 ======================================
		
      PIO(101)   <=  '0';  ------------------------------- Output_Enable (nach init vom ALTERA)
      PIO(119)   <=  '0';  ------------------------------- Output_Enable (nach init vom ALTERA)
      
      PIO(111)  <=  AWOut_Reg1(7);  --  Output "7"
      PIO(113)  <=  AWOut_Reg1(6);  --  Output "6"
      PIO(115)  <=  AWOut_Reg1(5);  --  Output "5"
      PIO(117)  <=  AWOut_Reg1(4);  --  Output "4"
      PIO(103)  <=  AWOut_Reg1(3);  --  Output "3"
      PIO(105)  <=  AWOut_Reg1(2);  --  Output "2"
      PIO(107)  <=  AWOut_Reg1(1);  --  Output "1"
      PIO(109)  <=  AWOut_Reg1(0);  --  Output "0"
		
	 	 

  WHEN   c_AW_HFIO =>
      
    --###################################################################################
    --####                  Anwender-IO: DA(DAC/ADC)  -- FG900_780                    ###
    --###################################################################################
  
      extension_cid_system <= 55; -- extension card: cid_system, CSCOHW=55
      extension_cid_group  <= 34; -- extension card: cid_group, "FG900780_HFIO1" = 34

		s_nLED_User1_i <= '0';				-- LED3 = User 1, -- frei -- 
		s_nLED_User2_i <= '0';				-- LED3 = User 2, -- frei -- 
		s_nLED_User3_i <= '0';				-- LED3 = User 3, -- frei -- 

		

	 --========================== Output Register 1 ======================================

													 --   +-- Pin-Nr. D-Sub37-F(J5)
													 --   |
      PIO(83)   <=  not AWOut_Reg1(12); --  14, OE12, "Strobe Amplitude"
      PIO(85)   <=  not AWOut_Reg1(11); --  13, OE11, "Amplitude-Bit11"
      PIO(87)   <=  not AWOut_Reg1(10); --  12, OE10, "Amplitude-Bit10"
      PIO(89)   <=  not AWOut_Reg1(9);  --  11, OE9,  "Amplitude-Bit9"
      PIO(91)   <=  not AWOut_Reg1(8);  --  10, OE8,  "Amplitude-Bit8"
      PIO(93)   <=  not AWOut_Reg1(7);  --  9,  OE7,  "Amplitude-Bit7"
      PIO(95)   <=  not AWOut_Reg1(6);  --  8,  OE6,  "Amplitude-Bit6"
      PIO(97)   <=  not AWOut_Reg1(5);  --  7,  OE5,  "Amplitude-Bit5"
      PIO(99)   <=  not AWOut_Reg1(4);  --  6,  OE4,  "Amplitude-Bit4"
      PIO(101)  <=  not AWOut_Reg1(3);  --  5,  OE3,  "Amplitude-Bit3"
      PIO(103)  <=  not AWOut_Reg1(2);  --  4,  OE2,  "Amplitude-Bit2"
      PIO(105)  <=  not AWOut_Reg1(1);  --  3,  OE1,  "Amplitude-Bit1"
      PIO(107)  <=  not AWOut_Reg1(0);  --  2,  OE0,  "Amplitude-Bit0"
                                                
                                                
		--========================== Output Register 2 ======================================
		
													 --   +-- Pin-Nr. D-Sub37-F(J5)
													 --   |
      PIO(61)   <=  not AWOut_Reg2(10); --  31, OC10, "Strobe Phase"
		PIO(63)   <=  not AWOut_Reg2(9);  --  30, OC9,  "Phase-Bit9"
      PIO(65)   <=  not AWOut_Reg2(8);  --  29, OC8,  "Phase-Bit8"
      PIO(67)   <=  not AWOut_Reg2(7);  --  28, OC7,  "Phase-Bit7"
      PIO(69)   <=  not AWOut_Reg2(6);  --  27, OC6,  "Phase-Bit6"
      PIO(71)   <=  not AWOut_Reg2(5);  --  26, OC5,  "Phase-Bit5"
      PIO(73)   <=  not AWOut_Reg2(4);  --  25, OC4,  "Phase-Bit4"
      PIO(75)   <=  not AWOut_Reg2(3);  --  24, OC3,  "Phase-Bit3"
      PIO(77)   <=  not AWOut_Reg2(2);  --  23, OC2,  "Phase-Bit2"
      PIO(79)   <=  not AWOut_Reg2(1);  --  22, OC1,  "Phase-Bit1"
      PIO(81)   <=  not AWOut_Reg2(0);  --  21, OC0,  "Phase-Bit0"



		--========================== Output Register 3 ======================================
		
		PIO(133)   <=  '0';  ---------------- Output_Enable (nach init vom ALTERA) für Tastpuls,Sample_Puls_Display und Reserve
		
		
      PIO(113)  			<=  not AWOut_Reg3(7); 		--  LEMO2(Frei, J7),  "Aux "
      HFIO_Aux_i			<= AWOut_Reg3(7);				--	Input  "nLED_Aux"
      PIO(35)   			<= HFIO_nLED_Aux_o;   		--	Output "nLED_Aux"

      PIO(59)   			<=  not AWOut_Reg3(6); 		--  D-Sub37-F(J5)_Pin-Nr. 32, OC11, "Flattop-Puls"

      PIO(111)  			<=  not AWOut_Reg3(5); 		--  Lemo4(Tastpuls),          OE15, "Tastpuls(Gating) "
      HFIO_Tastpuls_i	<= AWOut_Reg3(5);				--	Input  "nLED_Tastpuls"
      PIO(39)   			<= HFIO_nLED_Tastpuls_o;	--	Output "nLED_Tastpuls"

      PIO(57)   			<=  not AWOut_Reg3(4);		--  D-Sub37-F(J5)_Pin-Nr. 33, OC12, "Tast-Puls inv."
      PIO(55)   			<=  not AWOut_Reg3(3);		--  D-Sub37-F(J5)_Pin-Nr. 34, OC13, "Tast-Puls"
		
      PIO(115)  						<=  not AWOut_Reg3(2); 					--  LEMO1(Sample_Puls_Display, J8),       "Sample_Puls_Display"
      HFIO_Sample_Puls_Display_i	<= AWOut_Reg3(2);							--	Input  "nLED_Sample-Puls-Display"
      PIO(33)   						<= HFIO_nLED_Sample_Puls_Display_o;	--	Output "nLED_Sample_Puls_Display"

      PIO(51)   						<=  not AWOut_Reg3(1); 				--  LEMO3(Sample_Puls_inv, J6),     OC15, "Sample_Puls_inv. "
      HFIO_Sample_Puls_inv_i		<= AWOut_Reg3(1);						--	Input  "nLED_Sample_Puls_inv"
      PIO(37)   						<= HFIO_nLED_Sample_Puls_inv_o;	--	Output "nLED_Sample_Puls_inv"

		PIO(53)   <=  not AWOut_Reg3(0); --  D-Sub37-F(J5)_Pin-Nr. 35,       OC14, "Sample-Puls"

		

		--========================== Input Register 1 ======================================

      HFIO_in_AMP_FEHLER_Deb_i	<=	PIO(43);  							--  input "AMP_FEHLER"
		AWin1(1)							<=	HFIO_in_AMP_FEHLER_Deb_o;
      HFIO_in_PHASE_FEHLER_Deb_i	<=	PIO(41);  							--  input "PHASE_FEHLER"
		AWin1(0)							<=	HFIO_in_PHASE_FEHLER_Deb_o;

	
      
  WHEN OTHERS =>


		s_nLED_User1_i <= '0';				-- LED3 = User 1, -- frei -- 
		s_nLED_User2_i <= '0';				-- LED3 = User 2, -- frei -- 
		s_nLED_User3_i <= '0';				-- LED3 = User 3, -- frei -- 


  -- Output: Anwender-LED's ---

      PIO(17)   <=    clk_blink; -- LED7
      PIO(19)   <=    clk_blink; -- LED6   
      PIO(21)   <=    clk_blink; -- LED5     
      PIO(23)   <=    clk_blink; -- LED4   
      PIO(25)   <=    clk_blink; -- LED3   
      PIO(27)   <=    clk_blink; -- LED2      
      PIO(29)   <=    clk_blink; -- LED1      
      PIO(31)   <=    clk_blink; -- LED0   
           
  END CASE;


  END IF;
--
--  
END PROCESS p_AW_MUX;


end architecture;
