const child_process = require('child_process');

const demod = child_process.spawn(
    './rtl_app',
    [
        '-p'
    ],
    {
        stdio: [
            0,      // User parent stdin
            'pipe', // Pipe stdout
            'pipe'  // Pipe stderr
        ]
    }
);
const rds_decoder = child_process.spawn(
    'redsea',
    [
        '',
        '-r 190000'
    ],
    {
        stdio: [
            'pipe', // Pipe stdin
            'pipe', // Pipe stdout
            0       // Use parent stderr
        ]
    }
);

var rds_text = "";

demod.stdout.pipe(rds_decoder.stdin); // Send baseband data to the RDS decoder
demod.stderr.pipe(process.stdout); // Send the demodulator debug strings to the console

rds_decoder.stdout.on(
    'data',
    function (buf)
    {
        var obj = JSON.parse(buf);

        console.log(obj);

        /*
        if(obj.partial_radiotext)
        {
            process.stdout.write('\x1b[35m' + obj.partial_radiotext + '\r');
            //console.log(obj);
        }
        */

        if(obj.radiotext)
        {
            process.stdout.write('\x1b[35m' + obj.radiotext + '\r');
            //console.log(obj);
        }
    }
);