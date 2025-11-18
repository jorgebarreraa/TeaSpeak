import { CountryFlag } from "svg-sprites/country-flags";
import { AbstractTranslationResolver } from "tc-shared/i18n/Translation";
export declare type I18NContributor = {
    name: string;
    email: string;
};
export declare type TranslationResolverCreateResult = {
    status: "success";
    resolver: AbstractTranslationResolver;
} | {
    status: "error";
    message: string;
};
export declare abstract class I18NTranslation {
    abstract getId(): string;
    abstract getName(): string;
    abstract getCountry(): CountryFlag;
    abstract getDescription(): string;
    abstract getContributors(): I18NContributor[];
    abstract createTranslationResolver(): Promise<TranslationResolverCreateResult>;
}
export declare abstract class I18NRepository {
    abstract getName(): string;
    abstract getDescription(): string;
    abstract getTranslations(): Promise<I18NTranslation[]>;
}
