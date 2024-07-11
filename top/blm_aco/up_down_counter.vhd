LIBRARY IEEE;
USE IEEE.std_logic_1164.all;
USE IEEE.numeric_std.all;
 

entity up_down_counter is
    generic (
    	--c            : integer :=1;        -- Counter_input width
        WIDTH        : integer := 30      -- Counter width

        
    );
    port (
        CLK         : in std_logic;      -- Clock
        nRST         : in std_logic;      -- Reset
        CLEAR       : in std_logic;      -- Clear counter register
    --    LOAD        : in std_logic;      -- Load counter register
        ENABLE      : in std_logic;      -- Enable count operation
        UP_IN       : in std_logic;    -- Load counter register up input
        DOWN_IN     : in std_logic;    -- Load counter register down input
        pos_threshold: in integer;
        neg_threshold: in integer;
        cnt_val    : out std_logic_vector(WIDTH-1 downto 0);    --Counter register

        UP_OVERFLOW    : out std_logic ;     -- UP_Counter overflow
        DOWN_OVERFLOW    : out std_logic      -- UP_Counter overflow
    
    );
end up_down_counter;

architecture rtl of up_down_counter is
  --  signal Counter  : signed(WIDTH-1 downto 0);         -- up Counter register
 
  signal int_count: integer:=0;
    signal up_OVERFLOW_FLAG: std_logic:='0';
    signal  down_OVERFLOW_FLAG:std_logic:='0';
begin
    -- Counter process
    COUNT_SHIFT: process (nRST, CLK)
    begin
        if (nRST = '0') then    

          --  Counter <= (others => '0');                -- Reset counter register 
            up_OVERFLOW_FLAG <='0';
            down_OVERFLOW_FLAG <='0';
            int_count <=0;

            
        elsif (CLK'event and CLK='1') then
        
            if (CLEAR = '1') then

             --   Counter <= (others => '0');            -- Clear counter register
                int_count <= 0;
                up_OVERFLOW_FLAG <='0';
                down_OVERFLOW_FLAG <='0';

            elsif ( ENABLE = '1') then   
       
                if UP_IN ='1' then  --positive slope
                    if DOWN_IN ='0' then 
                        int_count <= int_count +1;
                    end if;
                elsif DOWN_IN ='1' then --negative slope
                    int_count <= int_count -1;
                end if;

                        
                if int_count = pos_threshold then         -- pos_threshold reached

                    up_OVERFLOW_FLAG <='1';
       
                elsif int_count = neg_threshold then  -- neg_threshold reached
             
                    down_OVERFLOW_FLAG <='1';                 
                
                end if;
            
            end if;
        end if;
 end process;
UP_OVERFLOW <= up_OVERFLOW_FLAG;
DOWN_OVERFLOW <= down_OVERFLOW_FLAG;
cnt_val <=  std_logic_vector(to_signed(int_count, WIDTH));


end rtl;

