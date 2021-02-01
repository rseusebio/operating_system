import React from "react";
import Chart from "react-google-charts";
import CircularProgress from "@material-ui/core/CircularProgress";
import simulator from "./simulation_records.json";

import "./App.css";


const getDate = (seconds: number) => {
  if (seconds > 60) {
    let minutes = Math.floor(seconds / 60);
    let remaining = seconds % 60;

    return new Date(0, 0, 0, 0, minutes, remaining, 0);
  }

  return new Date(0, 0, 0, 0, 0, seconds, 0);

}

const ProcessChart =  ( { arrays, title }: any ) => 
{
  arrays.sort((a, b) => {
    let a1 = parseInt(a[0] as string);
    let b1 = parseInt(b[0] as string);
    if (a1 < b1) {
      return -1;
    }
    else if (a1 > b1) {
      return 1;
    }
    return 0;
  });

  arrays = arrays.map((a => {
    return a.map(v => {
      if (typeof v == "number") {
        return getDate(v);
      }
      return "PID " + v;
    })
  }));

  return (
    <div className = "chart-container"> 

      <h4> 
        {
          title
        }
      </h4>

      <Chart
        
        width={"600px"}
        height={"500px"}
        chartType={"Timeline"}
        loader={<CircularProgress />}
        data={[
          [
            { type: 'string', id: 'Process' },
            { type: 'date', id: 'Start' },
            { type: 'date', id: 'End' },
          ],
          ...arrays
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

    </div>

  );
}

const App = ( _: any ) =>  
{
  return (
    <div>
      
      <ProcessChart title="CPU Timeline" arrays= {  simulator.cpu_records }/>

      <ProcessChart title="Disk Timeline" arrays= {  simulator.disk_records }/>

      <ProcessChart title="Printer Timeline" arrays= {  simulator.print_records }/>

      <ProcessChart title="Magnetic Tape Timeline" arrays= {  simulator.magnetic_records }/>

    </div>
  );
}

export default App;
