export class Version {
    major: number;
    minor: number;
    patch: number;
    build: number;

    timestamp: number;

    constructor(major: number, minor: number, patch: number, build: number, timestamp: number) {
        this.major = major;
        this.minor = minor;
        this.patch = patch;
        this.build = build;
        this.timestamp = timestamp;
    }

    toString(timestamp: boolean = false) {
        let result = "";
        result += this.major + ".";
        result += this.minor + ".";
        result += this.patch;
        if(this.build > 0) {
            result += "-" + this.build;
        }

        if(timestamp && this.timestamp > 0) {
            result += " [" + this.timestamp + "]";
        }
        return result;
    }


    equals(other: Version) : boolean {
        if(other == this) return true;
        if(typeof(other) != typeof(this)) return false;

        if(other.major != this.major) return false;
        if(other.minor != this.minor) return false;
        if(other.patch != this.patch) return false;
        if(other.build != this.build) return false;

        return other.timestamp === 0 || this.timestamp === 0 || other.timestamp === this.timestamp;
    }

    newerThan(other: Version, compareTimestamps?: boolean) : boolean {
        if(other.timestamp > 0 && this.timestamp > 0 && typeof compareTimestamps === "boolean" && compareTimestamps) {
            if(other.timestamp > this.timestamp) {
                return false;
            } else if(other.timestamp < this.timestamp) {
                return true;
            }
        }

        if(other.major > this.major) return false;
        else if(other.major < this.major) return true;

        if(other.minor > this.minor) return false;
        else if(other.minor < this.minor) return true;

        else if(other.patch < this.patch) return true;
        if(other.patch > this.patch) return false;

        if(other.build > this.build) return false;
        else if(other.build < this.build) return true;

        return false;
    }

    isDevelopmentVersion() : boolean {
        return this.build == 0 && this.major == 0 && this.minor == 0 && this.patch == 0;
    }
}

//1.0.0-2 [1000]
export function parseVersion(version: string) : Version {
    let result: Version = new Version(0, 0, 0, 0, 0);

    const roots = version.split(" ");
    {
        const parts = roots[0].split("-");
        const numbers = parts[0].split(".");

        if(numbers.length > 0) result.major = parseInt(numbers[0]);
        if(numbers.length > 1) result.minor = parseInt(numbers[1]);
        if(numbers.length > 2) result.patch = parseInt(numbers[2]);
        if(parts.length > 1) result.build = parseInt(parts[1]);
    }
    if(roots.length > 1 && ((roots[1] = roots[1].trim()).startsWith("[") && roots[1].endsWith("]"))) {
        result.timestamp = parseInt(roots[1].substr(1, roots[1].length - 2));
    }

    return result;
}