const addon = require('../build/Release/addon.node');

const DB_SIZE_KEY = '__dbsize__';
const AUX_FIELD_KEY = '__aux__';
const TYPES_KEY = '__types__';

// Add type mapping
const TYPE_NAMES = {
    0: 'string',
    1: 'list',
    2: 'set',
    3: 'zset',
    4: 'hash',
    6: 'stream'
};

let dbSizeDetails = null;
let auxFieldDetails = null;
let typesDetails = null;
let data = null;
try {
    const result = addon.parseRDB('./dump.rdb');
    try {
        const parsed = JSON.parse(result);
        
        // Process each object in the array
        parsed.forEach(obj => {
            if (obj[DB_SIZE_KEY]) {
                dbSizeDetails = obj[DB_SIZE_KEY];
            } else if (obj[AUX_FIELD_KEY]) {
                auxFieldDetails = obj[AUX_FIELD_KEY];
            } else if (obj[TYPES_KEY]) {
                typesDetails = obj[TYPES_KEY];
                // Object with types or multiple keys is our data object
            } else {
                data = obj;
            }
        });
        const keyLengths = [];
        // Process only the data keys, excluding types
        Object.entries(data).forEach(([key, value]) => {
            const type = TYPE_NAMES[typesDetails[key]];
            keyLengths.push({
                key: key,
                type: type,
                length: Array.isArray(value) ? value.length :
                        typeof value === 'string' ? value.length :
                        value.entries ? value.entries.length :
                        Object.keys(value).length,
            });
        });

        const sortedByKeyLength = keyLengths.sort((a, b) => b.length - a.length);
        console.log('DB details:', dbSizeDetails);
        console.log('AUX details:', auxFieldDetails);
        sortedByKeyLength.forEach(info => {
            console.log(`${info.key}:
                Type: ${info.type}
                Length: ${info.length}`);
        });
    } catch (e) {
        console.error('JSON Parse Error:', e.message);
        // Log the actual result for debugging
        console.error('Raw result:', result);
    }
} catch (error) {
    console.error('RDB Parse Error:', error.message);
}
