import React from "react";
import Chart from "react-google-charts";
import CircularProgress from "@material-ui/core/CircularProgress";

const getDate = ( seconds: number ) =>
{
  if( seconds > 60 )
  {
    let minutes = Math.floor( seconds / 60 );
    let remaining = seconds % 60;

    return new Date( 0, 0, 0, 0, minutes, remaining , 0 );
  }

  return new Date( 0, 0, 0, 0, 0, seconds , 0 );
  
}
const data = {
  arrays: [
    [ "2", getDate( 0 ), getDate( 2 ) ],
    [ "1", getDate( 3 ), getDate( 5 ) ],
    [ "2", getDate( 6 ), getDate( 8 ) ],
    [ "1", getDate( 11 ), getDate( 12 ) ],
    [ "2", getDate( 12 ), getDate( 15 ) ],
    [ "1", getDate( 18 ), getDate( 20 ) ],
    [ "1", getDate( 23 ), getDate( 26 ) ]
  ]
}

data.arrays.sort( ( a, b )=> {
  let a1 = parseInt( a[ 0 ] as string );
  let b1 = parseInt( b[ 0 ] as string );

  if( a1 < b1 )
  {
    return -1;
  }
  else if( a1 > b1 )
  {
    return 1;
  }
  
  return 0;
})

function App() {
  return (
    <div>

      <h6> Timeline chart </h6>

      <Chart
        width={ "500px" }
        height={ "500px" }
        chartType={ "Timeline" }
        loader={ <CircularProgress/> }
        data= {[
          [
            { type: 'string', id: 'President' },
            { type: 'date', id: 'Start' },
            { type: 'date', id: 'End' },
          ],
          ...data.arrays
        ]}
        options={
          {
            colorByRowLabel: true,
          }
        }
        rootProps={
          {
            'data-testid': '1'
          }
        }
      />
      <div>
        <h1>
          foot
        </h1>
      </div>
    </div>
  );
}

export default App;
