import "svg-sprites/country-flags";
import { CountryFlag } from "svg-sprites/country-flags";
interface CountryInfo {
    name: string;
    alpha_2: string;
    alpha_3: string;
    un_code: number;
    icon: string;
    flagMissingWarned?: boolean;
}
export declare function getKnownCountries(): CountryInfo[];
export declare function getCountryName(alphaCode: string, fallback?: string): string;
export declare function getCountryFlag(alphaCode: string): CountryFlag;
export {};
